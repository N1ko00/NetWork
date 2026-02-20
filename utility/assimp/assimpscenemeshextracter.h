// assimpscenemeshextracter.h
#pragma once

#include <assimp/material.h>
#include <assimp/scene.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include <stdexcept> // std::out_of_range

#include <cctype>        // std::isdigit
#include <limits>        // std::numeric_limits
#include <memory>        // std::shared_ptr
#include <unordered_map> // std::unordered_map


class AssimpSceneMeshExtractor
{
public:
    // -------------------------
    // Options
    // -------------------------
    struct Options
    {
        bool includeNormals = true;
        bool includeTangents = true;
        bool includeUV0 = true;
        bool includeColor0 = true;
        bool includeBones = true;

        bool flipUV_V = false;  // v = 1 - v
        bool toLeftHanded = true;   // RH->LH (Z反転 + winding反転)

        bool extractMaterials = true;   // MaterialData を抽出
        bool buildSubsets = true;   // 統合VB/IB + Subset を構築
    };

    // -------------------------
    // Vertex / Mesh / Subset
    // -------------------------
    struct Vertex
    {
        // Position
        float px{}, py{}, pz{};

        // Normal
        float nx{}, ny{}, nz{};

        // Tangent / Bitangent
        float tx{}, ty{}, tz{};
        float bx{}, by{}, bz{};

        // UV0
        float u{}, v{};

        // Color0
        float r{ 1 }, g{ 1 }, b{ 1 }, a{ 1 };

        // Bones (4 influences)
        std::uint32_t boneIndex[4]{ 0,0,0,0 };
        float         boneWeight[4]{ 0,0,0,0 };
    };

    struct MeshData
    {
        std::string name{};
        int materialIndex = -1; // aiMesh->mMaterialIndex

        std::vector<Vertex>        vertices{};
        std::vector<std::uint32_t> indices{};
    };

    struct Subset
    {
        std::string name{};
        int materialindex = -1;   // ★ このサブセットが参照するマテリアル番号（m_materials の index）

        std::uint32_t VertexBase = 0;
        std::uint32_t VertexNum = 0;

        std::uint32_t IndexBase = 0;
        std::uint32_t IndexNum = 0;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // MaterialData
    //   Assimp(aiMaterial) から抽出して保持する「マテリアル情報のスナップショット」。
    //   - Legacy(Phong系) と PBR(金属/粗さ) の両系統を同居させる
    //   - 「値を取得できたか」「テクスチャを取得できたか」を has-flag で明示する
    //   - Embedded texture(内蔵テクスチャ) かどうかも TextureRef で保持する
    // ─────────────────────────────────────────────────────────────────────────────
    struct MaterialData
    {
        // RGBA 4成分カラー（Assimp の aiColor4D 相当を安全に保持）
        struct Color4
        {
            float r{}, g{}, b{}, a{};   // r,g,b,a: 0..1 を想定（データ源によっては範囲外もあり得る）
        };

        // 1枚のテクスチャ参照情報
        //   - 外部ファイルのパス or 内蔵(Embedded) の参照名/インデックスを保持
        //   - どの aiTextureType で「ヒットしたか」を matchedType に残す（デバッグに有用）
//        struct TextureRef
//        {
//            std::string  path{};        // テクスチャの参照文字列（例: "albedo.png" / "*0" / "textureName" 等）
//            bool         isEmbedded{ false };        // scene->GetEmbeddedTexture で見つかる内蔵テクスチャなら true
//            int          embeddedIndex{ -1 };        // "*0" 形式のときのみ 0.. / 名前形式(例:"*tex") は -1
//            aiTextureType matchedType{ aiTextureType_NONE }; // ★どの aiTextureType で見つかったか（抽出側が設定）

//            // path が空でなければ「何かしら参照がある」とみなす簡易判定
//           bool HasTexture() const noexcept { return !path.empty(); }
//        };

/*
        struct TextureRef
        {
            std::string  path{};
            bool         isEmbedded{ false };
            int          embeddedIndex{ -1 };

            // ★追加：内蔵テクスチャを RGBA にしたもの
            int embeddedWidth{ 0 };
            int embeddedHeight{ 0 };
            std::vector<std::uint8_t> embeddedRGBA{}; // size = w*h*4

            aiTextureType matchedType{ aiTextureType_NONE }; // ★どの aiTextureType で見つかったか（抽出側が設定）

            // path が空でなければ「何かしら参照がある」とみなす簡易判定
            bool HasTexture() const noexcept { return !path.empty(); }

            bool HasEmbeddedRGBA() const noexcept
            {
                return isEmbedded && embeddedWidth > 0 && embeddedHeight > 0
                    && embeddedRGBA.size() == (std::size_t)embeddedWidth * embeddedHeight * 4;
            }
        };
*/

        struct TextureRef
        {
            std::string  path{};
            bool         isEmbedded{ false };
            int          embeddedIndex{ -1 };

            // ★追加：内蔵テクスチャを RGBA にしたもの（キャッシュ共有）
            int embeddedWidth{ 0 };
            int embeddedHeight{ 0 };
            std::shared_ptr<std::vector<std::uint8_t>> embeddedRGBA{}; // ★shared_ptr に変更

            aiTextureType matchedType{ aiTextureType_NONE };

            bool HasTexture() const noexcept { return !path.empty(); }

            bool HasEmbeddedRGBA() const noexcept
            {
                return isEmbedded
                    && embeddedWidth > 0 && embeddedHeight > 0
                    && embeddedRGBA
                    && embeddedRGBA->size() == (std::size_t)embeddedWidth * embeddedHeight * 4;
            }
        };

        std::string name{};             // マテリアル名（aiMaterial の名前など）

        // ─────────────────────────────────────────────────────────────────────────
        // Legacy values（従来の Phong/Blinn-Phong 等を想定した値群）
        // ※ PBR が無い/弱いデータでも、従来互換で描画できるよう保持する
        // ─────────────────────────────────────────────────────────────────────────
        Color4 diffuseColor{ 1,1,1,1 };     // 拡散色（Kd）
        Color4 specularColor{ 1,1,1,1 };    // 鏡面色（Ks）
        Color4 ambientColor{ 0,0,0,1 };     // 環境色（Ka）※現在は未使用なことも多い
        Color4 emissiveColor{ 0,0,0,1 };    // 自己発光色（Ke）※ Legacy 側の発光色

        float shininess{ 0.0f };            // 光沢(ハイライトの鋭さ) ※ Phong の指数など
        float opacity{ 1.0f };              // 不透明度（1=不透明 / 0=透明）
        float specularStrength{ 1.0f };     // 鏡面強度（Ks の倍率等）
        bool  twoSided{ false };            // 両面描画フラグ（カリング無効化相当）

        // ★ Legacy has-flags
        //   「値が取得できた(=ソースに明示的に存在した)」ことを示す。
        //   デフォルト値との区別ができるため、後段での補完/上書き判定に使える。
        bool hasDiffuseColor{ false };      // diffuseColor を取得できた
        bool hasSpecularColor{ false };     // specularColor を取得できた
        bool hasAmbientColor{ false };      // ambientColor を取得できた
        bool hasEmissiveColor{ false };     // emissiveColor を取得できた

        bool hasShininess{ false };         // shininess を取得できた
        bool hasOpacity{ false };           // opacity を取得できた
        bool hasSpecularStrength{ false };  // specularStrength を取得できた
        bool hasTwoSided{ false };          // twoSided を取得できた

        // Legacy系テクスチャが各スロットで取得できたか（取り回し用の簡易フラグ）
        bool hasLegacyDiffuseTex{ false };      // diffuseTex
        bool hasLegacySpecularTex{ false };     // specularTex
        bool hasLegacyAmbientTex{ false };      // ambientTex
        bool hasLegacyEmissiveTex{ false };     // emissiveTexLegacy

        // Legacyの「追加系」スロット（ファイルによっては用途が曖昧／流用されることがある）
        bool hasLegacyHeightTex{ false };       // heightTexLegacy（高さ/バンプ等）
        bool hasLegacyShinessTex{ false };      // shinessTexLegacy（綴りはそのまま）光沢/指数マップ等
        bool hasLegacyOpacityTex{ false };      // opacityTexLegacy（透過/アルファ）
        bool hasLegacyDisplaceTex{ false };     // displaceTexLegacy（ディスプレイス）
        bool hasLegacyLightmapTex{ false };     // lightmapTexLegacy（ライトマップ）
        bool hasLegacyReflectionTex{ false };   // reflectionTexLegacy（反射）
        bool hasLegacyBasecolorTex{ false };    // basecolorTexLegacy（Legacy側に basecolor が入ってくるケース用）
        bool hasLegacyNormalcameraTex{ false }; // normalcameraTexLegacy（法線/カメラ空間等、出所次第）
        bool hasLegacyEmissionTex{ false };     // emissionTexLegacy（Legacy側の発光テクスチャ）

        // ★ Legacy総合
        //   Legacy側で「何か1つでも」取得できたら true（描画経路選択/デバッグ用）
        bool hasAnyLegacy{ false };

        // ─────────────────────────────────────────────────────────────────────────
        // Legacy textures（従来スロットのテクスチャ参照）
        //   TextureRef は「パス/内蔵/インデックス/ヒットした type」まで持つ
        // ─────────────────────────────────────────────────────────────────────────
        TextureRef diffuseTex{};             // aiTextureType_DIFFUSE 等
        TextureRef specularTex{};            // aiTextureType_SPECULAR 等
        TextureRef ambientTex{};             // aiTextureType_AMBIENT 等
        TextureRef emissiveTexLegacy{};      // aiTextureType_EMISSIVE 等（Legacy側）

        TextureRef heightTexLegacy{};        // aiTextureType_HEIGHT 等
        TextureRef shinessTexLegacy{};       // aiTextureType_SHININESS 等（綴りはそのまま）
        TextureRef opacityTexLegacy{};       // aiTextureType_OPACITY 等
        TextureRef displaceTexLegacy{};      // aiTextureType_DISPLACEMENT 等
        TextureRef lightmapTexLegacy{};      // aiTextureType_LIGHTMAP 等
        TextureRef reflectionTexLegacy{};    // aiTextureType_REFLECTION 等
        TextureRef basecolorTexLegacy{};     // aiTextureType_BASE_COLOR 等（互換目的）
        TextureRef normalcameraTexLegacy{};  // aiTextureType_NORMAL_CAMERA 等
        TextureRef emissionTexLegacy{};      // aiTextureType_EMISSION_COLOR 等（Legacy側の保持用）

        // ─────────────────────────────────────────────────────────────────────────
        // PBR textures（金属度/粗さワークフローを想定）
        //   glTF2.0 等で一般的なスロットを保持
        // ─────────────────────────────────────────────────────────────────────────
        TextureRef baseColorTex{};           // BaseColor/Albedo
        TextureRef normalTex{};              // Normal
        TextureRef metallicTex{};            // Metallic
        TextureRef roughnessTex{};           // Roughness
        TextureRef occlusionTex{};           // AO(occlusion)
        TextureRef emissiveTex{};            // Emissive（PBR側）

        // ★ PBR texture has-flags（必要なら使えるように持っておく）
        //   テクスチャ参照が取得できたかどうか（抽出側が設定）
        bool hasPbrBaseColorTex{ false };    // baseColorTex
        bool hasPbrNormalTex{ false };       // normalTex
        bool hasPbrMetallicTex{ false };     // metallicTex
        bool hasPbrRoughnessTex{ false };    // roughnessTex
        bool hasPbrOcclusionTex{ false };    // occlusionTex
        bool hasPbrEmissiveTex{ false };     // emissiveTex

        // ─────────────────────────────────────────────────────────────────────────
        // PBR factors（テクスチャが無い/部分的な場合に使う係数）
        //   glTF の baseColorFactor / metallicFactor / roughnessFactor 等に対応
        // ─────────────────────────────────────────────────────────────────────────
        Color4 baseColorFactor{ 1,1,1,1 };   // ベースカラー係数（RGBA）
        float  metallicFactor{ 1.0f };       // 金属度係数
        float  roughnessFactor{ 1.0f };      // 粗さ係数

        bool hasBaseColorFactor{ false };    // baseColorFactor が明示的に取得できた
        bool hasMetallicFactor{ false };     // metallicFactor が明示的に取得できた
        bool hasRoughnessFactor{ false };    // roughnessFactor が明示的に取得できた

        Color4 emissiveFactor{ 0,0,0,1 };    // 発光係数（RGB を主に使用、A は保持）
        bool   hasEmissiveFactor{ false };   // emissiveFactor が明示的に取得できた

        // ★ PBR総合（PBR側で何か1つでも取れたら true）
        //   - PBRテクスチャ、PBR係数、その他PBR属性などの「いずれか」が取得できたら true
        bool hasAnyPBR{ false };
    };

public:
    void Clear();
    void Extract(const aiScene* scene, const Options& opt);

    // --------------------------------------------------------------------
    // Results (既存API)
    // --------------------------------------------------------------------
    const std::vector<MeshData>& Meshes()        const noexcept { return m_meshes; }
    const MeshData& GetMesh(std::size_t i) const { return m_meshes.at(i); }
    MeshData& GetMesh(std::size_t i) { return m_meshes.at(i); }


    const std::vector<Vertex>& SceneVertices() const noexcept { return m_sceneVertices; }
    const std::vector<std::uint32_t>& SceneIndices()  const noexcept { return m_sceneIndices; }
    const std::vector<Subset>& Subsets()       const noexcept { return m_subsets; }
    const std::vector<MaterialData>& Materials()     const noexcept { return m_materials; }

    // --------------------------------------------------------------------
    // Compatibility API (追加済み)
    // --------------------------------------------------------------------
    std::size_t MeshCount() const noexcept { return m_meshes.size(); }
    const std::vector<Subset>& SceneSubsets() const noexcept { return m_subsets; }

    const MaterialData& GetMaterial(std::size_t i) const { return m_materials.at(i); }
    MaterialData& GetMaterial(std::size_t i) { return m_materials.at(i); }

    // --------------------------------------------------------------------
    // ★ New: Valid material extractors
    // --------------------------------------------------------------------
    bool IsValidPBRMaterial(std::size_t i) const;
    bool IsValidLegacyMaterial(std::size_t i) const;

    std::vector<const MaterialData*> GetValidPBRMaterials() const;
    std::vector<const MaterialData*> GetValidLegacyMaterials() const;

private:
    // ---- vertex helpers ----
    static void AddBoneInfluence(Vertex& v, std::uint32_t boneIdx, float w);
    static void NormalizeBoneWeights(Vertex& v);

    static void ApplyLeftHanded(Vertex& v, bool affectTangent);
    static void FixWindingForLeftHanded(std::vector<std::uint32_t>& indices);

    // ---- material helpers ----
    static std::string GetMatName(const aiMaterial* mat);
    static std::string GetTexturePathFirst(const aiMaterial* mat, aiTextureType type);

    static bool TryGetColor4(const aiMaterial* mat,
        const char* pKey, unsigned int type, unsigned int idx,
        MaterialData::Color4& out);

    static bool TryGetFloat(const aiMaterial* mat,
        const char* pKey, unsigned int type, unsigned int idx,
        float& out);

    static bool TryGetBoolInt(const aiMaterial* mat,
        const char* pKey, unsigned int type, unsigned int idx,
        bool& out);

    MaterialData::TextureRef MakeTextureRef(
        const aiScene* scene,
        const aiMaterial* mat,
        std::initializer_list<aiTextureType> types);

    // ---- mesh extraction ----
    static MeshData ExtractOneMesh(const aiMesh* mesh, const Options& opt);

    const aiTexture* GetEmbeddedTexturePreferIndex(
        const aiScene* scene,
        const char* path,
        int embeddedIndex);
 
    static int ParseEmbeddedIndex(const char* path);

    static bool BuildEmbeddedRGBAFromAiTexture(
        const aiTexture* tex,
        int& outW, int& outH,
        std::vector<std::uint8_t>& outRGBA);

private:
    std::vector<MeshData>       m_meshes{};
    std::vector<Vertex>         m_sceneVertices{};
    std::vector<std::uint32_t>  m_sceneIndices{};
    std::vector<Subset>         m_subsets{};
    std::vector<MaterialData>   m_materials{};

private:
    struct EmbeddedImage
    {
        int w{ 0 };
        int h{ 0 };
        std::shared_ptr<std::vector<std::uint8_t>> rgba; // RGBA( w*h*4 )
    };

    // "*0" 形式（index）用キャッシュ
    std::unordered_map<int, EmbeddedImage> m_embeddedIndexCache{};

    // 名前形式用キャッシュ（GetEmbeddedTexture(name) でヒットする系）
    std::unordered_map<std::string, EmbeddedImage> m_embeddedNameCache{};
};
