#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "assimpscenemeshextracter.h"

#pragma pack(push, 1)

// version=1: [Header][Vertices][Indices]
// version=2: [Header][Vertices][Indices][Subsets]
struct MeshBinHeader
{
    char     magic[8];        // "GM31MESH"
    uint32_t version;         // 1 or 2

    uint32_t vertexStride;    // sizeof(AssimpSceneMeshExtractor::Vertex)
    uint32_t indexStride;     // sizeof(uint32_t)

    uint64_t vertexCount;
    uint64_t indexCount;

    uint32_t subsetStride;    // sizeof(SubsetBin) (v2)
    uint32_t subsetCount;     // (v2)

    uint32_t reserved[6];     // future
};

// AssimpSceneMeshExtractor::Subset と整合させる（固定長）
struct SubsetBin
{
    int32_t  materialindex;

    uint32_t VertexBase;
    uint32_t VertexNum;

    uint32_t IndexBase;
    uint32_t IndexNum;

    char     name[64]; // UTF-8, null終端（無ければ64全部）
};

#pragma pack(pop)

class SceneMeshBinaryWriter
{
public:
    // ex.SceneVertices / SceneIndices / SceneSubsets を書き出す
    bool Write(const AssimpSceneMeshExtractor& ex, const std::filesystem::path& outPath) const;
};

class SceneMeshBinaryReader
{
public:
    // 読み込み（version=1なら outSubsets は空）
    bool Read(const std::filesystem::path& path,
        std::vector<AssimpSceneMeshExtractor::Vertex>& outVertices,
        std::vector<std::uint32_t>& outIndices,
        std::vector<AssimpSceneMeshExtractor::Subset>& outSubsets) const;
};
