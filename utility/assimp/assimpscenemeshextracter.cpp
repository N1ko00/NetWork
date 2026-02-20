// AssimpSceneMeshExtractor.cpp

#include "assimpscenemeshextracter.h"
#include "../../system/stb_image.h"
#include <assimp/mesh.h>
#include <assimp/pbrmaterial.h> // AI_MATKEY_* PBR keys

#include <algorithm>
#include <cassert>
#include <cstdlib>   // std::strtol
#include <cstring>   // std::strlen, std::strncmp

// ============================================================
// この .cpp がやっていること（全体像）
// ============================================================
// 1) Extract(scene,opt)
//    - scene->mMaterials を走査して MaterialData を構築（LEGACY / PBR 両方）
//    - scene->mMeshes を走査して MeshData を構築（頂点・インデックス・ボーン）
//    - opt.buildSubsets が true のとき統合VB/IB + Subset を生成
//
// 2) MaterialData の中身
//    - LEGACY: diffuse/specular/ambient/emissive, shininess/opacity, テクスチャ群
//    - PBR: baseColor/normal/metallic/roughness/occlusion/emissive, factor群
//    - embedded 判定＆（embeddedなら）RGBA展開（stb_image）
//
// 3) 左手系変換（opt.toLeftHanded）
//    - 座標Z反転 + winding反転（表裏維持）

namespace
{
    // ------------------------------------------------------------
    // aiColor4D (Assimp) -> MaterialData::Color4（自前）への変換
    // ------------------------------------------------------------
    static AssimpSceneMeshExtractor::MaterialData::Color4 ToColor4(const aiColor4D& c)
    {
        return { c.r, c.g, c.b, c.a };
    }

    // ------------------------------------------------------------
    // 内蔵テクスチャを指すパスか？（Assimpの慣例："*0" など）
    // ------------------------------------------------------------
    static bool IsEmbeddedPath(const std::string& path)
    {
        return !path.empty() && path[0] == '*';
    }

    // ------------------------------------------------------------
    // "*0" 形式の embedded index を取り出す
    // - "*12" -> 12
    // - 失敗時は -1
    // ------------------------------------------------------------
    static int ParseEmbeddedIndex_StarNumber(const std::string& path)
    {
        // "*0" 形式のみ index を返す。失敗は -1。
        if (!IsEmbeddedPath(path)) return -1;
        char* end = nullptr;
        const long v = std::strtol(path.c_str() + 1, &end, 10);
        if (end == (path.c_str() + 1)) return -1;
        if (v < 0) return -1;
        return static_cast<int>(v);
    }

    // ------------------------------------------------------------
    // scene が “その embedded texture を持っているか” を判定
    // - "*0" の場合：scene->mTextures[index] を直接確認
    // - 名前形式の場合：scene->GetEmbeddedTexture(name) が non-null か
    // ------------------------------------------------------------
    static bool SceneHasEmbeddedTexture(const aiScene* scene, const std::string& path, int embeddedIndex)
    {
        if (!scene) return false;

        if (embeddedIndex >= 0)
        {
            return (scene->mNumTextures > static_cast<unsigned>(embeddedIndex)) &&
                (scene->mTextures[embeddedIndex] != nullptr);
        }

        // 名前形式の内蔵テクスチャ (例: "embedded:..." 等)
        return (scene->GetEmbeddedTexture(path.c_str()) != nullptr);
    }
}

const aiTexture* AssimpSceneMeshExtractor::GetEmbeddedTexturePreferIndex(
    const aiScene* scene,
    const char* path,
    int embeddedIndex)
{
    if (!scene) return nullptr;

    // まず "*0" 形式の index が有効なら、mTextures[index] を優先
    if (embeddedIndex >= 0)
    {
        const unsigned idx = static_cast<unsigned>(embeddedIndex);
        if (idx < scene->mNumTextures && scene->mTextures[idx])
            return scene->mTextures[idx];
    }

    // 次に名前形式含めて GetEmbeddedTexture を試す
    if (path && path[0] != '\0')
        return scene->GetEmbeddedTexture(path);

    return nullptr;
}

// ============================================================
// "*数字" 形式の埋め込みテクスチャ参照を index(int) に変換する共通関数
//
// Assimp では、埋め込みテクスチャ (aiScene::mTextures) を参照する際に
//   "*0", "*1", "*2" ...
// のような文字列パスで表現されることがあります。
// 例: aiMaterial::GetTexture(...) が返すパスが "*0" のとき、
//     scene->mTextures[0] が実体になります。
//
// 戻り値:
//   - 0以上 : 埋め込みテクスチャ配列のインデックス（"*0" → 0）
//   - -1    : 変換できない/対象外（null, '*' で始まらない, 数字が無い, 範囲外 など）
//
// 注意点:
//   - "*" の直後に10進数字が続くケースのみを有効として扱う。
//   - "*" 以外の形式（例: "foo.png" や "*something"）は -1。
//   - strtol は先頭から読める分だけ読むので、"*12abc" のような場合は
//     12 を返してしまう可能性があります（必要なら厳密チェックを追加）。
// ============================================================
int AssimpSceneMeshExtractor::ParseEmbeddedIndex(const char* path)
{
    // null なら当然失敗
    if (!path) return -1;

    // 埋め込み参照は "*" で始まる前提。それ以外は対象外として失敗。
    if (path[0] != '*') return -1;

    // strtol の変換終了位置を受け取るためのポインタ。
    // 変換に成功すると end は "数字の直後" を指す。
    char* end = nullptr;

    // "*" の次の文字から 10進数として変換する。
    // 例: path="*12" → v=12, end は '\0' を指す
    //     path="*0"  → v=0
    long v = std::strtol(path + 1, &end, 10);

    // end == path+1 なら「1文字も数字として読めなかった」＝数字なし
    // 例: "*" や "*abc" など
    if (end == path + 1) return -1;

    // long → int へ落とす前に範囲チェック。
    // (負数や int の上限超えは不正として扱う)
    if (v < 0 || v > 0x7fffffff) return -1;

    // 安全に int に変換して返す
    return static_cast<int>(v);
}


// ============================================================
// 内部バッファを全クリア
// - Extract() の最初に呼ばれて前回結果を破棄する
// ============================================================

void AssimpSceneMeshExtractor::Clear()
{
    m_meshes.clear();
    m_sceneVertices.clear();
    m_sceneIndices.clear();
    m_subsets.clear();
    m_materials.clear();

    // ★追加（embedded RGBA キャッシュ）
    m_embeddedIndexCache.clear();
    m_embeddedNameCache.clear();
}


// ============================================================
// Extract : AssimpのaiSceneから、メッシュ・マテリアルを抽出
// ============================================================
// opt.extractMaterials : true なら scene->mMaterials から MaterialData を作る
// opt.buildSubsets     : true なら 統合VB/IB + Subset 情報を作る
// その他 opt            : 頂点属性の有無、左手系変換、UV反転などを制御
// ============================================================
void AssimpSceneMeshExtractor::Extract(const aiScene* scene, const Options& opt)
{
    if (!scene)
        throw std::invalid_argument("AssimpSceneMeshExtractor::Extract: scene is null");

    Clear();

    // ============================================================
    // Materials (LEGACY / PBR)
    // ============================================================
    // - Assimpのmaterial数だけ MaterialData を作る
    // - LEGACY / PBR 両方の値を（取れたら）詰め、hasフラグを立てる
    // - テクスチャは MakeTextureRef() で path + isEmbedded 等を判定
    if (opt.extractMaterials && scene->HasMaterials() && scene->mNumMaterials > 0)
    {
        m_materials.reserve(scene->mNumMaterials);

        for (unsigned mi = 0; mi < scene->mNumMaterials; ++mi)
        {
            const aiMaterial* mat = scene->mMaterials[mi];
            MaterialData md{};
            md.name = GetMatName(mat);

            // -------------------------
            // LEGACY values
            // -------------------------
            // “昔ながら” の phong/lambert などで使うパラメータ群
            // - 色（diffuse/specular/ambient/emissive）
            // - shininess / opacity / specularStrength
            // - two-sided
            // - legacy texture types
            {
                MaterialData::Color4 c{};
                if (TryGetColor4(mat, AI_MATKEY_COLOR_DIFFUSE, c))
                {
                    md.diffuseColor = c; md.hasDiffuseColor = true;
                }
                if (TryGetColor4(mat, AI_MATKEY_COLOR_SPECULAR, c))
                {
                    md.specularColor = c; md.hasSpecularColor = true;
                }
                if (TryGetColor4(mat, AI_MATKEY_COLOR_AMBIENT, c))
                {
                    md.ambientColor = c; md.hasAmbientColor = true;
                }
                if (TryGetColor4(mat, AI_MATKEY_COLOR_EMISSIVE, c))
                {
                    md.emissiveColor = c; md.hasEmissiveColor = true;
                }

                float f{};
                if (TryGetFloat(mat, AI_MATKEY_SHININESS, f))
                {
                    md.shininess = f; md.hasShininess = true;
                }
                if (TryGetFloat(mat, AI_MATKEY_OPACITY, f))
                {
                    md.opacity = f; md.hasOpacity = true;
                }
                if (TryGetFloat(mat, AI_MATKEY_SHININESS_STRENGTH, f))
                {
                    md.specularStrength = f; md.hasSpecularStrength = true;
                }

                bool b{};
                if (TryGetBoolInt(mat, AI_MATKEY_TWOSIDED, b))
                {
                    md.twoSided = b; md.hasTwoSided = true;
                }

                // Legacy textures
                // ※ここで渡している aiTextureType_* のリストの “先頭から順に” 見つかったものを採用
                // ※MakeTextureRef 内で embedded 判定・embedded RGBA 展開も（sceneがあれば）実施
                md.diffuseTex = MakeTextureRef(scene, mat, { aiTextureType_DIFFUSE });
                md.specularTex = MakeTextureRef(scene, mat, { aiTextureType_SPECULAR });
                md.ambientTex = MakeTextureRef(scene, mat, { aiTextureType_AMBIENT });
                md.emissiveTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_EMISSIVE });

                md.heightTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_HEIGHT });
                md.shinessTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_SHININESS });
                md.opacityTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_OPACITY });
                md.displaceTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_DISPLACEMENT });
                md.lightmapTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_LIGHTMAP });
                md.reflectionTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_REFLECTION });
                md.basecolorTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_BASE_COLOR });
                md.normalcameraTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_NORMAL_CAMERA });
                md.emissionTexLegacy = MakeTextureRef(scene, mat, { aiTextureType_EMISSION_COLOR });

                // “テクスチャパスが空でない” を hasフラグに反映
                md.hasLegacyDiffuseTex = md.diffuseTex.HasTexture();
                md.hasLegacySpecularTex = md.specularTex.HasTexture();
                md.hasLegacyAmbientTex = md.ambientTex.HasTexture();
                md.hasLegacyEmissiveTex = md.emissiveTexLegacy.HasTexture();

                md.hasLegacyHeightTex = md.heightTexLegacy.HasTexture();
                md.hasLegacyShinessTex = md.shinessTexLegacy.HasTexture();
                md.hasLegacyOpacityTex = md.opacityTexLegacy.HasTexture();
                md.hasLegacyDisplaceTex = md.displaceTexLegacy.HasTexture();
                md.hasLegacyLightmapTex = md.lightmapTexLegacy.HasTexture();
                md.hasLegacyReflectionTex = md.reflectionTexLegacy.HasTexture();
                md.hasLegacyBasecolorTex = md.basecolorTexLegacy.HasTexture();
                md.hasLegacyNormalcameraTex = md.normalcameraTexLegacy.HasTexture();
                md.hasLegacyEmissionTex = md.emissionTexLegacy.HasTexture();

            }

            // -------------------------
            // PBR (Metallic-Roughness / glTF2想定)
            // -------------------------
            // - AssimpがPBRをどの aiTextureType に割り当てるかはインポータ/設定で変わり得るため
            //   「優先 + fallback」の順序を initializer_list で渡して吸収している
            {
                // PBR textures（Assimp が適切な aiTextureType に割り当てる想定）
                // baseColor は BASE_COLOR を優先、無ければ DIFFUSE も fallback で見る
                md.baseColorTex = MakeTextureRef(scene, mat, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE });

                // normal は NORMALS を優先、無ければ HEIGHT を fallback
                md.normalTex = MakeTextureRef(scene, mat, { aiTextureType_NORMALS, aiTextureType_HEIGHT });

                md.metallicTex = MakeTextureRef(scene, mat, { aiTextureType_METALNESS });
                md.roughnessTex = MakeTextureRef(scene, mat, { aiTextureType_DIFFUSE_ROUGHNESS });
                md.occlusionTex = MakeTextureRef(scene, mat, { aiTextureType_AMBIENT_OCCLUSION });
                md.emissiveTex = MakeTextureRef(scene, mat, { aiTextureType_EMISSIVE });

                // hasフラグを設定
                md.hasPbrBaseColorTex = md.baseColorTex.HasTexture();
                md.hasPbrNormalTex = md.normalTex.HasTexture();
                md.hasPbrMetallicTex = md.metallicTex.HasTexture();
                md.hasPbrRoughnessTex = md.roughnessTex.HasTexture();
                md.hasPbrOcclusionTex = md.occlusionTex.HasTexture();
                md.hasPbrEmissiveTex = md.emissiveTex.HasTexture();

                // PBR factors（複数キーを順に試す）
                // - AssimpのPBRキーは複数存在し得る（glTF専用キーなど）
                // - 取れたら hasXXX を立てる
                MaterialData::Color4 c{};
                if (TryGetColor4(mat, AI_MATKEY_BASE_COLOR, c) ||
                    TryGetColor4(mat, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, c))

                {
                    md.baseColorFactor = c;
                    md.hasBaseColorFactor = true;
                }
                else
                {
                    // fallback: legacy diffuse
                    if (TryGetColor4(mat, AI_MATKEY_COLOR_DIFFUSE, c))
                    {
                        md.baseColorFactor = c;
                        md.hasBaseColorFactor = true;
                    }
                }

                float f{};
                if (TryGetFloat(mat, AI_MATKEY_METALLIC_FACTOR, f) ||
                    TryGetFloat(mat, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, f))
                {
                    md.metallicFactor = f;
                    md.hasMetallicFactor = true;
                }

                if (TryGetFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, f) ||
                    TryGetFloat(mat, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, f))
                {
                    md.roughnessFactor = f;
                    md.hasRoughnessFactor = true;
                }

                // emissiveFactor は legacy emissive を流用（Assimpの一般的キー）
                MaterialData::Color4 e{};
                if (TryGetColor4(mat, AI_MATKEY_COLOR_EMISSIVE, e))
                {
                    md.emissiveFactor = e;
                    md.hasEmissiveFactor = true;
                }
            }

            // -------------------------
            // totals
            // -------------------------
            // “このMaterialがLEGACYとして意味を持つか / PBRとして意味を持つか” のまとめフラグ
            // ※描画側で「PBR優先」「なければLegacy」などの分岐に使える
            md.hasAnyLegacy =
                md.hasDiffuseColor || md.hasSpecularColor || md.hasAmbientColor || md.hasEmissiveColor ||
                md.hasShininess || md.hasOpacity || md.hasSpecularStrength || md.hasTwoSided ||
                md.hasLegacyDiffuseTex || md.hasLegacySpecularTex || md.hasLegacyAmbientTex || md.hasLegacyEmissiveTex;

            md.hasAnyPBR =
                md.hasPbrBaseColorTex || md.hasPbrNormalTex || md.hasPbrMetallicTex ||
                md.hasPbrRoughnessTex || md.hasPbrOcclusionTex || md.hasPbrEmissiveTex ||
                md.hasBaseColorFactor || md.hasMetallicFactor || md.hasRoughnessFactor || md.hasEmissiveFactor;

            // 完成した MaterialData を配列へ
            m_materials.push_back(std::move(md));
        }
    }

    // ============================================================
    // Meshes
    // ============================================================
    // - scene->mMeshes を MeshData 化
    // - buildSubsets なら統合VB/IB + Subset生成
    if (!scene->HasMeshes() || scene->mNumMeshes == 0)
        return;

    m_meshes.reserve(scene->mNumMeshes);

    // SUBSET/統合VBIBを作る場合は、できれば合計を先に見てreserve
    // - push_back/insert を多用するので、事前reserveで再確保を減らす
    if (opt.buildSubsets)
    {
        m_subsets.reserve(scene->mNumMeshes);

        std::size_t totalV = 0;
        std::size_t totalI = 0;
        for (unsigned m = 0; m < scene->mNumMeshes; ++m)
        {
            const aiMesh* mesh = scene->mMeshes[m];
            if (!mesh) continue;
            totalV += mesh->mNumVertices;

            // Triangulate前提なら faces*3 が目安
            totalI += static_cast<std::size_t>(mesh->mNumFaces) * 3;
        }

        m_sceneVertices.reserve(totalV);
        m_sceneIndices.reserve(totalI);
    }

    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!mesh) continue;

        // 1メッシュ分の抽出（頂点・インデックス・ボーンなど）
        MeshData md = ExtractOneMesh(mesh, opt);

        // メッシュ単位で保持
        m_meshes.push_back(std::move(md));

        // 統合VB/IB + SUBSET
        if (opt.buildSubsets)
        {
            const MeshData& last = m_meshes.back();

            // Subsetは「統合バッファのどこに、このメッシュが入っているか」を示す
            // - VertexBase / VertexNum
            // - IndexBase  / IndexNum
            // - materialindex（マテリアル番号）
            // - name（デバッグ/表示用）
            Subset sub{};
            // bug            sub.name = last.name;
            //            sub.materialindex = last.materialIndex;

            // 名前：aiMesh名があればそれを使う。なければ "mesh_番号"
            if (mesh && mesh->mName.length > 0)
                sub.name = mesh->mName.C_Str();
            else
                sub.name = "mesh_" + std::to_string(m);

            // マテリアル番号：範囲内なら mMaterialIndex を採用。範囲外なら -1
            sub.materialindex = (mesh && mesh->mMaterialIndex < scene->mNumMaterials)
                ? static_cast<int>(mesh->mMaterialIndex)
                : -1;

            // 現在の統合頂点数が、このメッシュの頂点開始位置になる
            sub.VertexBase = static_cast<std::uint32_t>(m_sceneVertices.size());
            sub.VertexNum = static_cast<std::uint32_t>(last.vertices.size());

            // 現在の統合インデックス数が、このメッシュのインデックス開始位置になる
            sub.IndexBase = static_cast<std::uint32_t>(m_sceneIndices.size());
            sub.IndexNum = static_cast<std::uint32_t>(last.indices.size());

            // 統合VBへ追加（このメッシュの頂点を末尾に連結）
            m_sceneVertices.insert(
                m_sceneVertices.end(),
                last.vertices.begin(),
                last.vertices.end());

            // indices は「メッシュローカル」のまま統合IBへ追加
            // 描画時：DrawIndexed(sub.IndexNum, sub.IndexBase, (INT)sub.VertexBase)
            // - BaseVertexLocation に VertexBase を渡すので、index自体はローカルのままでOK
            m_sceneIndices.insert(
                m_sceneIndices.end(),
                last.indices.begin(),
                last.indices.end());

            // Subset配列に追加
            m_subsets.push_back(std::move(sub));
        }
    }
}

// --------------------------------------------------------------------
// ★ Valid material extractors
// --------------------------------------------------------------------
// “このマテリアルはPBRとして有効か？/ Legacyとして有効か？” を返す
// - hasAnyPBR / hasAnyLegacy をそのまま返す設計
bool AssimpSceneMeshExtractor::IsValidPBRMaterial(std::size_t i) const
{
    const auto& m = m_materials.at(i);
    return m.hasAnyPBR;
}

bool AssimpSceneMeshExtractor::IsValidLegacyMaterial(std::size_t i) const
{
    const auto& m = m_materials.at(i);
    return m.hasAnyLegacy;
}

// 有効なPBR MaterialDataへのポインタ一覧（参照用）
// - out の中身は m_materials の要素アドレス
// - m_materials の再配置（reserve不足など）でアドレスが変わる可能性がある点に注意
std::vector<const AssimpSceneMeshExtractor::MaterialData*> AssimpSceneMeshExtractor::GetValidPBRMaterials() const
{
    std::vector<const MaterialData*> out;
    out.reserve(m_materials.size());
    for (const auto& m : m_materials)
    {
        if (m.hasAnyPBR) out.push_back(&m);
    }
    return out;
}

// 有効なLegacy MaterialDataへのポインタ一覧（参照用）
std::vector<const AssimpSceneMeshExtractor::MaterialData*> AssimpSceneMeshExtractor::GetValidLegacyMaterials() const
{
    std::vector<const MaterialData*> out;
    out.reserve(m_materials.size());
    for (const auto& m : m_materials)
    {
        if (m.hasAnyLegacy) out.push_back(&m);
    }
    return out;
}

// --------------------------------------------------------------------
// vertex helpers
// --------------------------------------------------------------------

// 1頂点に対して（最大4本の）ボーン影響を詰める
// - 空きスロット（weight==0）を優先
// - 埋まっていれば最小weightを置換（より大きいweightなら差し替える）
void AssimpSceneMeshExtractor::AddBoneInfluence(Vertex& v, std::uint32_t boneIdx, float w)
{
    if (w <= 0.0f) return;

    // 空きスロット優先
    for (int i = 0; i < 4; ++i)
    {
        if (v.boneWeight[i] == 0.0f)
        {
            v.boneIndex[i] = boneIdx;
            v.boneWeight[i] = w;
            return;
        }
    }

    // 4枠が埋まっていたら最小ウェイトを置換
    int minSlot = 0;
    for (int i = 1; i < 4; ++i)
        if (v.boneWeight[i] < v.boneWeight[minSlot]) minSlot = i;

    if (w > v.boneWeight[minSlot])
    {
        v.boneIndex[minSlot] = boneIdx;
        v.boneWeight[minSlot] = w;
    }
}

// 4本の weight 合計が 1 になるよう正規化
// - sum==0 の場合は 0 クリア（無影響扱い）
void AssimpSceneMeshExtractor::NormalizeBoneWeights(Vertex& v)
{
    const float sum =
        v.boneWeight[0] + v.boneWeight[1] + v.boneWeight[2] + v.boneWeight[3];

    if (sum > 0.0f)
    {
        for (auto& w : v.boneWeight) w /= sum;
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            v.boneIndex[i] = 0;
            v.boneWeight[i] = 0.0f;
        }
    }
}

// 右手系→左手系変換（この実装では Z反転）
// - position.z と normal.z は必ず反転
// - tangent/bitangent も使う場合は tz/bz も反転（affectTangent が true）
void AssimpSceneMeshExtractor::ApplyLeftHanded(Vertex& v, bool affectTangent)
{
    // 右手->左手: Z反転
    v.pz = -v.pz;
    v.nz = -v.nz;

    if (affectTangent)
    {
        v.tz = -v.tz;
        v.bz = -v.bz;
    }
}

// 左手系変換時に winding（頂点順）を入れ替え、表裏を維持する
// - 三角形 (i, i+1, i+2) の (1) と (2) を swap
void AssimpSceneMeshExtractor::FixWindingForLeftHanded(std::vector<std::uint32_t>& indices)
{
    // 三角形の (1) と (2) を入れ替えて winding を反転
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        std::swap(indices[i + 1], indices[i + 2]);
}

// --------------------------------------------------------------------
// material helpers
// --------------------------------------------------------------------

// マテリアル名を取得（AI_MATKEY_NAME）
// - 取得失敗なら空文字
std::string AssimpSceneMeshExtractor::GetMatName(const aiMaterial* mat)
{
    if (!mat) return {};
    aiString s;
    if (AI_SUCCESS == mat->Get(AI_MATKEY_NAME, s))
        return std::string(s.C_Str());
    return {};
}

// 指定 aiTextureType の “0番目” のパスを取得
// - テクスチャが無い or 取得失敗なら空文字
std::string AssimpSceneMeshExtractor::GetTexturePathFirst(const aiMaterial* mat, aiTextureType type)
{
    if (!mat) return {};
    if (mat->GetTextureCount(type) == 0)
        return {};

    aiString path;
    if (AI_SUCCESS == mat->GetTexture(type, 0, &path))
        return std::string(path.C_Str());

    return {};
}

// aiMaterial::Get で color4 を取る汎用ヘルパ
// - 成功したら out に詰めて true
bool AssimpSceneMeshExtractor::TryGetColor4(
    const aiMaterial* mat,
    const char* pKey, unsigned int type, unsigned int idx,
    MaterialData::Color4& out)
{
    if (!mat) return false;
    aiColor4D c;
    if (AI_SUCCESS == mat->Get(pKey, type, idx, c))
    {
        out = ToColor4(c);
        return true;
    }
    return false;
}

// aiMaterial::Get で float を取る汎用ヘルパ
bool AssimpSceneMeshExtractor::TryGetFloat(
    const aiMaterial* mat,
    const char* pKey, unsigned int type, unsigned int idx,
    float& out)
{
    if (!mat) return false;
    float f = 0.0f;
    if (AI_SUCCESS == mat->Get(pKey, type, idx, f))
    {
        out = f;
        return true;
    }
    return false;
}

// aiMaterial::Get で “intとして保存されているbool” を取る汎用ヘルパ
// - Assimpの two-sided などは int で入っていることが多い
bool AssimpSceneMeshExtractor::TryGetBoolInt(
    const aiMaterial* mat,
    const char* pKey, unsigned int type, unsigned int idx,
    bool& out)
{
    if (!mat) return false;
    int v = 0;
    if (AI_SUCCESS == mat->Get(pKey, type, idx, v))
    {
        out = (v != 0);
        return true;
    }
    return false;
}

/*
AssimpSceneMeshExtractor::MaterialData::TextureRef
AssimpSceneMeshExtractor::MakeTextureRef(
    const aiScene* scene,
    const aiMaterial* mat,
    std::initializer_list<aiTextureType> types)
{
    MaterialData::TextureRef tr{};

    if (!mat) return tr;

    for (aiTextureType t : types)
    {
        const std::string path = GetTexturePathFirst(mat, t);
        if (path.empty())
            continue;

        tr.path = path;
        tr.matchedType = t;

        // 内蔵判定（"*0"形式 / 名前形式）
        const int embeddedIdx = ParseEmbeddedIndex_StarNumber(path);
        tr.embeddedIndex = embeddedIdx;

        if (SceneHasEmbeddedTexture(scene, path, embeddedIdx))
            tr.isEmbedded = true;
        else
            tr.isEmbedded = false; // 外部テクスチャ

        return tr;
    }

    return tr;
}

*/

// ============================================================
// embedded aiTexture を RGBA(8bit*4) に展開する
// ============================================================
// Assimp の aiTexture 仕様：
// - tex->mHeight == 0 : 圧縮データ（png/jpg等）。tex->mWidth が “バイト数”
// - tex->mHeight  > 0 : 非圧縮（aiTexel配列）。mWidth×mHeight の画素
//
// 圧縮の場合：stb_image で decode して RGBA 化
// 非圧縮の場合：aiTexel(BGRA) -> RGBAへ並べ替えて格納
bool AssimpSceneMeshExtractor::BuildEmbeddedRGBAFromAiTexture(
    const aiTexture* tex,
    int& outW, int& outH,
    std::vector<std::uint8_t>& outRGBA)
{
    outW = 0; outH = 0;
    outRGBA.clear();

    if (!tex) return false;

    // Assimp仕様：
    // - tex->mHeight == 0 : 圧縮データ（PNG/JPG等）。tex->mWidth がバイト数。
    // - tex->mHeight  > 0 : 非圧縮。tex->pcData に aiTexel (BGRA) が並ぶ。
    if (tex->mHeight == 0)
    {
        // compressed
        const auto* bytes = reinterpret_cast<const unsigned char*>(tex->pcData);
        const int len = static_cast<int>(tex->mWidth);
        if (!bytes || len <= 0) return false;

        int w = 0, h = 0, comp = 0;
        unsigned char* pixels = stbi_load_from_memory(bytes, len, &w, &h, &comp, STBI_rgb_alpha);
        if (!pixels) return false;

        outW = w;
        outH = h;

        const std::size_t size = static_cast<std::size_t>(w) * h * 4;
        outRGBA.resize(size);
        std::memcpy(outRGBA.data(), pixels, size);

        stbi_image_free(pixels);
        return true;
    }
    else
    {
        // uncompressed aiTexel array (BGRA)
        const int w = static_cast<int>(tex->mWidth);
        const int h = static_cast<int>(tex->mHeight);
        if (w <= 0 || h <= 0) return false;

        const std::size_t pixelCount = static_cast<std::size_t>(w) * h;
        if (!tex->pcData) return false;

        outW = w;
        outH = h;
        outRGBA.resize(pixelCount * 4);

        for (std::size_t p = 0; p < pixelCount; ++p)
        {
            const aiTexel& t = tex->pcData[p];
            // aiTexel は BGRA
            outRGBA[p * 4 + 0] = t.r;
            outRGBA[p * 4 + 1] = t.g;
            outRGBA[p * 4 + 2] = t.b;
            outRGBA[p * 4 + 3] = t.a;
        }
        return true;
    }
}

// ============================================================
// MakeTextureRef : 指定type群から最初に見つかったテクスチャをTextureRef化
// ============================================================
// - mat->GetTexture(type, 0, &str) で 0番目だけを見る
// - path を保存し、matchedType を保存
// - "*0" 形式なら embeddedIndex を保存
// - scene があれば scene->GetEmbeddedTexture(path) で embedded 判定し、
//   embeddedなら RGBA展開して embeddedRGBA / embeddedWidth/Height に詰める

AssimpSceneMeshExtractor::MaterialData::TextureRef
AssimpSceneMeshExtractor::MakeTextureRef(
    const aiScene* scene,
    const aiMaterial* mat,
    std::initializer_list<aiTextureType> types)
{
    MaterialData::TextureRef out{};

    if (!mat) return out;

    for (aiTextureType type : types)
    {
        aiString str;
        if (mat->GetTexture(type, 0, &str) != AI_SUCCESS)
            continue;

        const char* p = str.C_Str();
        if (!p || p[0] == '\0')
            continue;

        out.path = p;
        out.matchedType = type;

        // (3) 厳密 "*数字" パース
        out.embeddedIndex = ParseEmbeddedIndex(p);

        // scene が無いならここまで
        if (!scene)
            break;

        // (2) index優先で embedded を取得
        const aiTexture* tex = GetEmbeddedTexturePreferIndex(scene, p, out.embeddedIndex);
        if (!tex)
        {
            out.isEmbedded = false;
            break; // 参照はあるが embedded ではない（外部ファイルなど）
        }

        out.isEmbedded = true;

        // (4) キャッシュキー：indexが有効なら index cache、そうでなければ name cache
        if (out.embeddedIndex >= 0)
        {
            auto it = m_embeddedIndexCache.find(out.embeddedIndex);
            if (it != m_embeddedIndexCache.end())
            {
                // キャッシュヒット：共有参照を渡すだけ
                out.embeddedWidth = it->second.w;
                out.embeddedHeight = it->second.h;
                out.embeddedRGBA = it->second.rgba;
                break;
            }

            // キャッシュミス：展開して格納
            int w = 0, h = 0;
            std::vector<std::uint8_t> rgba;
            EmbeddedImage entry{};

            if (BuildEmbeddedRGBAFromAiTexture(tex, w, h, rgba))
            {
                entry.w = w;
                entry.h = h;
                entry.rgba = std::make_shared<std::vector<std::uint8_t>>(std::move(rgba));

                out.embeddedWidth = entry.w;
                out.embeddedHeight = entry.h;
                out.embeddedRGBA = entry.rgba;
            }
            else
            {
                // embedded だが展開失敗
                entry.w = 0;
                entry.h = 0;
                entry.rgba.reset();
                out.embeddedWidth = 0;
                out.embeddedHeight = 0;
                out.embeddedRGBA.reset();
            }

            m_embeddedIndexCache.emplace(out.embeddedIndex, std::move(entry));
            break;
        }
        else
        {
            const std::string key = out.path;

            auto it = m_embeddedNameCache.find(key);
            if (it != m_embeddedNameCache.end())
            {
                out.embeddedWidth = it->second.w;
                out.embeddedHeight = it->second.h;
                out.embeddedRGBA = it->second.rgba;
                break;
            }

            int w = 0, h = 0;
            std::vector<std::uint8_t> rgba;
            EmbeddedImage entry{};

            if (BuildEmbeddedRGBAFromAiTexture(tex, w, h, rgba))
            {
                entry.w = w;
                entry.h = h;
                entry.rgba = std::make_shared<std::vector<std::uint8_t>>(std::move(rgba));

                out.embeddedWidth = entry.w;
                out.embeddedHeight = entry.h;
                out.embeddedRGBA = entry.rgba;
            }
            else
            {
                entry.w = 0;
                entry.h = 0;
                entry.rgba.reset();
                out.embeddedWidth = 0;
                out.embeddedHeight = 0;
                out.embeddedRGBA.reset();
            }

            m_embeddedNameCache.emplace(key, std::move(entry));
            break;
        }
    }

    return out;
}

/*
AssimpSceneMeshExtractor::MaterialData::TextureRef
AssimpSceneMeshExtractor::MakeTextureRef(
    const aiScene* scene,
    const aiMaterial* mat,
    std::initializer_list<aiTextureType> types)
{
    MaterialData::TextureRef out{};

    if (!mat) return out;

    for (aiTextureType type : types)
    {
        aiString str;
        if (mat->GetTexture(type, 0, &str) != AI_SUCCESS)
            continue;

        const char* p = str.C_Str();
        if (!p || p[0] == '\0')
            continue;

        out.path = p;
        out.matchedType = type;

        // (3) 厳密 "*数字" パース
        out.embeddedIndex = ParseEmbeddedIndex(p);

        // scene が無いならここまで
        if (!scene)
            break;

        // (2) index優先で embedded を取得
        const aiTexture* tex = GetEmbeddedTexturePreferIndex(scene, p, out.embeddedIndex);
        if (!tex)
        {
            out.isEmbedded = false;
            break; // 参照はあるが embedded ではない（外部ファイルなど）
        }

        out.isEmbedded = true;

        // (4) キャッシュキー：indexが有効なら index cache、そうでなければ name cache
        if (out.embeddedIndex >= 0)
        {
            auto it = m_embeddedIndexCache.find(out.embeddedIndex);
            if (it != m_embeddedIndexCache.end())
            {
                // キャッシュヒット：共有参照を渡すだけ
                out.embeddedWidth = it->second.w;
                out.embeddedHeight = it->second.h;
                out.embeddedRGBA = it->second.rgba;
                break;
            }

            // キャッシュミス：展開して格納
            int w = 0, h = 0;
            std::vector<std::uint8_t> rgba;
            EmbeddedImage entry{};

            if (BuildEmbeddedRGBAFromAiTexture(tex, w, h, rgba))
            {
                entry.w = w;
                entry.h = h;
                entry.rgba = std::make_shared<std::vector<std::uint8_t>>(std::move(rgba));

                out.embeddedWidth = entry.w;
                out.embeddedHeight = entry.h;
                out.embeddedRGBA = entry.rgba;
            }
            else
            {
                // embedded だが展開失敗
                entry.w = 0;
                entry.h = 0;
                entry.rgba.reset();
                out.embeddedWidth = 0;
                out.embeddedHeight = 0;
                out.embeddedRGBA.reset();
            }

            m_embeddedIndexCache.emplace(out.embeddedIndex, std::move(entry));
            break;
        }
        else
        {
            const std::string key = out.path;

            auto it = m_embeddedNameCache.find(key);
            if (it != m_embeddedNameCache.end())
            {
                out.embeddedWidth = it->second.w;
                out.embeddedHeight = it->second.h;
                out.embeddedRGBA = it->second.rgba;
                break;
            }

            int w = 0, h = 0;
            std::vector<std::uint8_t> rgba;
            EmbeddedImage entry{};

            if (BuildEmbeddedRGBAFromAiTexture(tex, w, h, rgba))
            {
                entry.w = w;
                entry.h = h;
                entry.rgba = std::make_shared<std::vector<std::uint8_t>>(std::move(rgba));

                out.embeddedWidth = entry.w;
                out.embeddedHeight = entry.h;
                out.embeddedRGBA = entry.rgba;
            }
            else
            {
                entry.w = 0;
                entry.h = 0;
                entry.rgba.reset();
                out.embeddedWidth = 0;
                out.embeddedHeight = 0;
                out.embeddedRGBA.reset();
            }

            m_embeddedNameCache.emplace(key, std::move(entry));
            break;
        }
    }

    return out;
}
*/

/*
AssimpSceneMeshExtractor::MaterialData::TextureRef
AssimpSceneMeshExtractor::MakeTextureRef(
    const aiScene* scene,
    const aiMaterial* mat,
    std::initializer_list<aiTextureType> types)
{
    MaterialData::TextureRef out{};

    if (!mat) return out;

    // まず “見つかった最初のtype” を採用
    for (aiTextureType type : types)
    {
        aiString str;
        if (mat->GetTexture(type, 0, &str) != AI_SUCCESS)
            continue;

        const char* p = str.C_Str();
        if (!p || p[0] == '\0')
            continue;

        out.path = p;
        out.matchedType = type;

        // "*0" 形式なら index を保存
        out.embeddedIndex = ParseEmbeddedIndex(p);

        // scene があれば embedded 判定＆RGBA生成
        if (scene)
        {
            const aiTexture* tex = scene->GetEmbeddedTexture(p);
            if (tex)
            {
                out.isEmbedded = true;

                // ★ここで RGBA を生成して詰める
                int w = 0, h = 0;
                std::vector<std::uint8_t> rgba;
                if (BuildEmbeddedRGBAFromAiTexture(tex, w, h, rgba))
                {
                    out.embeddedWidth = w;
                    out.embeddedHeight = h;
                    out.embeddedRGBA = std::move(rgba);
                }
                else
                {
                    // embeddedだが展開失敗：フラグだけ立てておく（デバッグ用）
                    out.embeddedWidth = 0;
                    out.embeddedHeight = 0;
                    out.embeddedRGBA.clear();
                }
            }
        }

        // 最初に見つかったものを採用して終了
        break;
    }

    return out;
}
*/

// --------------------------------------------------------------------
// mesh extraction
// --------------------------------------------------------------------

// ============================================================
// ExtractOneMesh : aiMesh 1つを MeshData に変換
// ============================================================
// - vertices : optで指定された属性だけ埋める（法線/接線/UV/Color等）
// - bones    : opt.includeBones なら最大4影響で詰め、最後に正規化
// - indices  : face の indices をそのまま並べる（Triangulate前提）
// - 左手系   : Z反転 + winding反転
AssimpSceneMeshExtractor::MeshData
AssimpSceneMeshExtractor::ExtractOneMesh(const aiMesh* mesh, const Options& opt)
{
    assert(mesh);

    MeshData out{};
    out.name = mesh->mName.C_Str();
    out.materialIndex = static_cast<int>(mesh->mMaterialIndex);

    // --- vertices ---
    out.vertices.resize(mesh->mNumVertices);

    // opt と aiMesh の状態から “実際にこのメッシュが持っているか” を確定
    const bool hasNormals = opt.includeNormals && mesh->HasNormals();
    const bool hasTangents = opt.includeTangents && mesh->HasTangentsAndBitangents();
    const bool hasUV0 = opt.includeUV0 && mesh->HasTextureCoords(0);
    const bool hasColor0 = opt.includeColor0 && mesh->HasVertexColors(0);

    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex v{};

        // Position
        v.px = mesh->mVertices[i].x;
        v.py = mesh->mVertices[i].y;
        v.pz = mesh->mVertices[i].z;

        // Normal
        if (hasNormals)
        {
            v.nx = mesh->mNormals[i].x;
            v.ny = mesh->mNormals[i].y;
            v.nz = mesh->mNormals[i].z;
        }

        // Tangent / Bitangent
        if (hasTangents)
        {
            v.tx = mesh->mTangents[i].x;
            v.ty = mesh->mTangents[i].y;
            v.tz = mesh->mTangents[i].z;

            v.bx = mesh->mBitangents[i].x;
            v.by = mesh->mBitangents[i].y;
            v.bz = mesh->mBitangents[i].z;
        }

        // UV0
        if (hasUV0)
        {
            v.u = mesh->mTextureCoords[0][i].x;
            v.v = mesh->mTextureCoords[0][i].y;
            if (opt.flipUV_V) v.v = 1.0f - v.v;
        }

        // Color0
        if (hasColor0)
        {
            const auto& c = mesh->mColors[0][i];
            v.r = c.r; v.g = c.g; v.b = c.b; v.a = c.a;
        }

        // Left-handed conversion (optional)
        if (opt.toLeftHanded)
            ApplyLeftHanded(v, hasTangents);

        out.vertices[i] = v;
    }

    // --- bones (optional) ---
    // 注意：boneIndex は「このメッシュ内のボーン番号（0..mNumBones-1）」です。
    // - もしスケルトン全体でのボーンIDが必要なら別のマッピングが必要
    if (opt.includeBones && mesh->HasBones())
    {
        for (unsigned b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            const std::uint32_t boneIndex = static_cast<std::uint32_t>(b);

            for (unsigned w = 0; w < bone->mNumWeights; ++w)
            {
                const aiVertexWeight& vw = bone->mWeights[w];
                const unsigned vid = vw.mVertexId;
                if (vid < out.vertices.size())
                    AddBoneInfluence(out.vertices[vid], boneIndex, vw.mWeight);
            }
        }

        // 最後に4本合計=1へ正規化
        for (auto& v : out.vertices)
            NormalizeBoneWeights(v);
    }

    // --- indices ---
    // faceごとに indices を詰める
    // aiProcess_Triangulate を前提に “3ずつ” になる想定だが、
    // 念のため mNumIndices 分すべて詰めている
    out.indices.reserve(mesh->mNumFaces * 3);

    for (unsigned f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];

        // aiProcess_Triangulate を前提（それ以外でも一応全部積む）
        for (unsigned k = 0; k < face.mNumIndices; ++k)
            out.indices.push_back(static_cast<std::uint32_t>(face.mIndices[k]));
    }

    // 左手系の場合、winding反転（表裏維持）
    if (opt.toLeftHanded)
        FixWindingForLeftHanded(out.indices);

    return out;
}
