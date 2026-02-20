#include "SceneMeshBinaryIO.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <type_traits>

static bool IsMagicOk(const char magic[8])
{
    return std::memcmp(magic, "GM31MESH", 8) == 0;
}

static void WriteNameUtf8Fixed(char outName[64], const std::string& in)
{
    std::memset(outName, 0, 64);
    if (in.empty()) return;

    // null終端確保（最大63+終端）
    const size_t n = std::min<size_t>(in.size(), 63);
    std::memcpy(outName, in.data(), n);
    outName[n] = '\0';
}

static std::string ReadNameUtf8Fixed(const char name[64])
{
    size_t n = 0;
    while (n < 64 && name[n] != '\0') ++n;
    return std::string(name, name + n);
}

// ==========================
// Writer
// ==========================
bool SceneMeshBinaryWriter::Write(const AssimpSceneMeshExtractor& ex,
    const std::filesystem::path& outPath) const
{
    using Vertex = AssimpSceneMeshExtractor::Vertex;
    using Subset = AssimpSceneMeshExtractor::Subset;

    static_assert(std::is_trivially_copyable_v<Vertex>,
        "Vertex must be trivially copyable to dump raw bytes safely.");
    static_assert(std::is_trivially_copyable_v<MeshBinHeader>,
        "MeshBinHeader must be trivially copyable.");
    static_assert(std::is_trivially_copyable_v<SubsetBin>,
        "SubsetBin must be trivially copyable.");

    const auto& v = ex.SceneVertices();
    const auto& i = ex.SceneIndices();
    const auto& s = ex.SceneSubsets();

    // subset を file 用に変換
    std::vector<SubsetBin> subsets;
    subsets.reserve(s.size());
    for (const Subset& ss : s)
    {
        SubsetBin sb{};
        sb.materialindex = static_cast<int32_t>(ss.materialindex);

        sb.VertexBase = ss.VertexBase;
        sb.VertexNum = ss.VertexNum;

        sb.IndexBase = ss.IndexBase;
        sb.IndexNum = ss.IndexNum;

        WriteNameUtf8Fixed(sb.name, ss.name);

        subsets.push_back(sb);
    }

    const bool hasSubsets = !subsets.empty();

    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) return false;

    MeshBinHeader h{};
    std::memcpy(h.magic, "GM31MESH", 8);

    h.version = hasSubsets ? 2u : 1u;
    h.vertexStride = static_cast<uint32_t>(sizeof(Vertex));
    h.indexStride = static_cast<uint32_t>(sizeof(uint32_t));

    h.vertexCount = static_cast<uint64_t>(v.size());
    h.indexCount = static_cast<uint64_t>(i.size());

    h.subsetStride = hasSubsets ? static_cast<uint32_t>(sizeof(SubsetBin)) : 0u;
    h.subsetCount = hasSubsets ? static_cast<uint32_t>(subsets.size()) : 0u;

    // [Header]
    ofs.write(reinterpret_cast<const char*>(&h), sizeof(h));
    if (!ofs) return false;

    // [Vertices]
    if (!v.empty())
    {
        ofs.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(Vertex)));
        if (!ofs) return false;
    }

    // [Indices]
    if (!i.empty())
    {
        ofs.write(reinterpret_cast<const char*>(i.data()),
            static_cast<std::streamsize>(i.size() * sizeof(uint32_t)));
        if (!ofs) return false;
    }

    // [Subsets] (v2)
    if (hasSubsets)
    {
        ofs.write(reinterpret_cast<const char*>(subsets.data()),
            static_cast<std::streamsize>(subsets.size() * sizeof(SubsetBin)));
        if (!ofs) return false;
    }

    return true;
}

// ==========================
// Reader
// ==========================
bool SceneMeshBinaryReader::Read(const std::filesystem::path& path,
    std::vector<AssimpSceneMeshExtractor::Vertex>& outVertices,
    std::vector<std::uint32_t>& outIndices,
    std::vector<AssimpSceneMeshExtractor::Subset>& outSubsets) const
{
    using Vertex = AssimpSceneMeshExtractor::Vertex;
    using Subset = AssimpSceneMeshExtractor::Subset;

    static_assert(std::is_trivially_copyable_v<Vertex>,
        "Vertex must be trivially copyable to load raw bytes safely.");

    outVertices.clear();
    outIndices.clear();
    outSubsets.clear();

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    MeshBinHeader h{};
    ifs.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!ifs) return false;

    if (!IsMagicOk(h.magic)) return false;
    if (h.version != 1 && h.version != 2) return false;

    if (h.vertexStride != sizeof(Vertex)) return false;
    if (h.indexStride != sizeof(uint32_t)) return false;

    // 簡易ガード（必要なら上限調整）
    constexpr uint64_t kMaxCount = 1ull << 30;
    if (h.vertexCount > kMaxCount || h.indexCount > kMaxCount) return false;

    if (h.version == 2)
    {
        if (h.subsetCount > kMaxCount) return false;
        if (h.subsetStride != sizeof(SubsetBin)) return false; // 将来拡張なら >= にする
    }

    // [Vertices]
    outVertices.resize(static_cast<size_t>(h.vertexCount));
    if (!outVertices.empty())
    {
        ifs.read(reinterpret_cast<char*>(outVertices.data()),
            static_cast<std::streamsize>(outVertices.size() * sizeof(Vertex)));
        if (!ifs) return false;
    }

    // [Indices]
    outIndices.resize(static_cast<size_t>(h.indexCount));
    if (!outIndices.empty())
    {
        ifs.read(reinterpret_cast<char*>(outIndices.data()),
            static_cast<std::streamsize>(outIndices.size() * sizeof(uint32_t)));
        if (!ifs) return false;
    }

    // [Subsets] (v2)
    if (h.version == 2 && h.subsetCount > 0)
    {
        outSubsets.resize(static_cast<size_t>(h.subsetCount));

        for (size_t n = 0; n < outSubsets.size(); ++n)
        {
            SubsetBin sb{};
            ifs.read(reinterpret_cast<char*>(&sb), sizeof(sb));
            if (!ifs) return false;

            Subset s{};
            s.materialindex = static_cast<int>(sb.materialindex);

            s.VertexBase = sb.VertexBase;
            s.VertexNum = sb.VertexNum;

            s.IndexBase = sb.IndexBase;
            s.IndexNum = sb.IndexNum;

            s.name = ReadNameUtf8Fixed(sb.name);

            outSubsets[n] = std::move(s);
        }
    }

    return true;
}
