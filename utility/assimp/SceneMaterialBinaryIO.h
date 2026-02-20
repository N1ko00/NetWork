// SceneMaterialBinaryIO.h
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "assimpscenemeshextracter.h"

#pragma pack(push, 1)
struct MaterialBinHeader
{
    char     magic[8];     // "GM31MATL"
    uint32_t version;      // 1
    uint32_t materialCount;
    uint32_t reserved[13]; // future (64 bytes total)
};
#pragma pack(pop)

class SceneMaterialBinaryWriter
{
public:
    // ex.Materials() をそのまま書き出す（PBR/LEGACY/埋め込み判定フラグ含む）
    bool Write(const AssimpSceneMeshExtractor& ex, const std::filesystem::path& outPath) const;

    // 既に持っている MaterialData 群を直接書く
    bool Write(const std::vector<AssimpSceneMeshExtractor::MaterialData>& materials,
        const std::filesystem::path& outPath) const;
};

class SceneMaterialBinaryReader
{
public:
    // ファイルから読み込み、MaterialData 配列として返す（元クラスへは注入しない）
    bool Read(const std::filesystem::path& path,
        std::vector<AssimpSceneMeshExtractor::MaterialData>& outMaterials) const;

    // 読み込んだ配列から「有効PBR/有効LEGACY」だけを取得
    static std::vector<const AssimpSceneMeshExtractor::MaterialData*>
        GetValidPBRMaterials(const std::vector<AssimpSceneMeshExtractor::MaterialData>& materials);

    static std::vector<const AssimpSceneMeshExtractor::MaterialData*>
        GetValidLegacyMaterials(const std::vector<AssimpSceneMeshExtractor::MaterialData>& materials);
};
