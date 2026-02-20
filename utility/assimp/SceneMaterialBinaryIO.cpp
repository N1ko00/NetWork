// SceneMaterialBinaryIO.cpp
#include "SceneMaterialBinaryIO.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

using MaterialData = AssimpSceneMeshExtractor::MaterialData;

static bool IsMagicOk(const char magic[8])
{
    return std::memcmp(magic, "GM31MATL", 8) == 0;
}

static bool WriteBytes(std::ofstream& ofs, const void* data, std::uint32_t size)
{
    if (size == 0) return true;
    ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(ofs);
}

static bool ReadBytes(std::ifstream& ifs, void* data, std::uint32_t size)
{
    if (size == 0) return true;
    ifs.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(ifs);
}

static bool WriteU8(std::ofstream& ofs, std::uint8_t v)
{
    ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    return static_cast<bool>(ofs);
}
static bool WriteU32(std::ofstream& ofs, std::uint32_t v)
{
    ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    return static_cast<bool>(ofs);
}
static bool WriteI32(std::ofstream& ofs, std::int32_t v)
{
    ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    return static_cast<bool>(ofs);
}
static bool WriteF32(std::ofstream& ofs, float v)
{
    ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    return static_cast<bool>(ofs);
}

static bool ReadU8(std::ifstream& ifs, std::uint8_t& v)
{
    ifs.read(reinterpret_cast<char*>(&v), sizeof(v));
    return static_cast<bool>(ifs);
}
static bool ReadU32(std::ifstream& ifs, std::uint32_t& v)
{
    ifs.read(reinterpret_cast<char*>(&v), sizeof(v));
    return static_cast<bool>(ifs);
}
static bool ReadI32(std::ifstream& ifs, std::int32_t& v)
{
    ifs.read(reinterpret_cast<char*>(&v), sizeof(v));
    return static_cast<bool>(ifs);
}
static bool ReadF32(std::ifstream& ifs, float& v)
{
    ifs.read(reinterpret_cast<char*>(&v), sizeof(v));
    return static_cast<bool>(ifs);
}

static bool WriteString(std::ofstream& ofs, const std::string& s)
{
    // [u32 byteLen][bytes]
    if (s.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    const std::uint32_t len = static_cast<std::uint32_t>(s.size());
    if (!WriteU32(ofs, len)) return false;
    if (len > 0)
    {
        ofs.write(s.data(), static_cast<std::streamsize>(len));
        if (!ofs) return false;
    }
    return true;
}

static bool ReadString(std::ifstream& ifs, std::string& out)
{
    std::uint32_t len = 0;
    if (!ReadU32(ifs, len)) return false;

    // ガード（必要なら調整）
    constexpr std::uint32_t kMaxStr = 4u * 1024u * 1024u; // 4MB
    if (len > kMaxStr) return false;

    out.clear();
    if (len == 0) return true;

    out.resize(len);
    ifs.read(out.data(), static_cast<std::streamsize>(len));
    return static_cast<bool>(ifs);
}

static bool WriteColor4(std::ofstream& ofs, const MaterialData::Color4& c)
{
    return WriteF32(ofs, c.r) && WriteF32(ofs, c.g) && WriteF32(ofs, c.b) && WriteF32(ofs, c.a);
}

static bool ReadColor4(std::ifstream& ifs, MaterialData::Color4& c)
{
    return ReadF32(ifs, c.r) && ReadF32(ifs, c.g) && ReadF32(ifs, c.b) && ReadF32(ifs, c.a);
}

static bool WriteTextureRef(std::ofstream& ofs, const MaterialData::TextureRef& t, std::uint32_t version)
{
    // v1 fields
    if (!WriteString(ofs, t.path)) return false;
    if (!WriteU8(ofs, static_cast<std::uint8_t>(t.isEmbedded ? 1 : 0))) return false;
    if (!WriteI32(ofs, static_cast<std::int32_t>(t.embeddedIndex))) return false;
    if (!WriteI32(ofs, static_cast<std::int32_t>(t.matchedType))) return false;

    // v2 fields: embedded RGBA payload
    if (version >= 2u)
    {
        const bool has = t.HasEmbeddedRGBA();

        const std::int32_t w = has ? static_cast<std::int32_t>(t.embeddedWidth) : 0;
        const std::int32_t h = has ? static_cast<std::int32_t>(t.embeddedHeight) : 0;

        if (!WriteI32(ofs, w)) return false;
        if (!WriteI32(ofs, h)) return false;

        std::uint32_t bytes = 0;
        if (has)
        {
            const auto& buf = *t.embeddedRGBA;
            if (buf.size() > std::numeric_limits<std::uint32_t>::max()) return false;
            bytes = static_cast<std::uint32_t>(buf.size());
        }

        if (!WriteU32(ofs, bytes)) return false;
        if (bytes > 0)
        {
            const auto& buf = *t.embeddedRGBA;
            if (!WriteBytes(ofs, buf.data(), bytes)) return false;
        }
    }

    return true;
}

static bool ReadTextureRef(std::ifstream& ifs, MaterialData::TextureRef& t, std::uint32_t version)
{
    // v1 fields
    if (!ReadString(ifs, t.path)) return false;

    std::uint8_t emb = 0;
    if (!ReadU8(ifs, emb)) return false;
    t.isEmbedded = (emb != 0);

    std::int32_t eidx = -1;
    if (!ReadI32(ifs, eidx)) return false;
    t.embeddedIndex = static_cast<int>(eidx);

    std::int32_t mt = 0;
    if (!ReadI32(ifs, mt)) return false;
    t.matchedType = static_cast<aiTextureType>(mt);

    // defaults
    t.embeddedWidth = 0;
    t.embeddedHeight = 0;
    t.embeddedRGBA.reset();   // ★shared_ptrなのでreset

    // v2 fields
    if (version >= 2u)
    {
        std::int32_t w = 0, h = 0;
        if (!ReadI32(ifs, w)) return false;
        if (!ReadI32(ifs, h)) return false;

        std::uint32_t bytes = 0;
        if (!ReadU32(ifs, bytes)) return false;

        // guard（必要なら調整）
        constexpr std::uint32_t kMaxTexBytes = 512u * 1024u * 1024u; // 512MB
        if (bytes > kMaxTexBytes) return false;

        if (bytes > 0)
        {
            auto buf = std::make_shared<std::vector<std::uint8_t>>();
            buf->resize(bytes);

            if (!ReadBytes(ifs, buf->data(), bytes)) return false;

            t.embeddedWidth = static_cast<int>(w);
            t.embeddedHeight = static_cast<int>(h);
            t.embeddedRGBA = std::move(buf);
        }
    }

    return true;
}

static bool WriteBool(std::ofstream& ofs, bool b)
{
    return WriteU8(ofs, static_cast<std::uint8_t>(b ? 1 : 0));
}
static bool ReadBool(std::ifstream& ifs, bool& b)
{
    std::uint8_t v = 0;
    if (!ReadU8(ifs, v)) return false;
    b = (v != 0);
    return true;
}

// ==========================
// Writer
// ==========================
bool SceneMaterialBinaryWriter::Write(const AssimpSceneMeshExtractor& ex, const std::filesystem::path& outPath) const
{
    return Write(ex.Materials(), outPath);
}

bool SceneMaterialBinaryWriter::Write(const std::vector<MaterialData>& materials,
    const std::filesystem::path& outPath) const
{
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) return false;

    MaterialBinHeader h{};
    std::memcpy(h.magic, "GM31MATL", 8);
    h.version = 2u;
    h.materialCount = static_cast<std::uint32_t>(materials.size());

    ofs.write(reinterpret_cast<const char*>(&h), sizeof(h));
    if (!ofs) return false;

    const std::uint32_t ver = h.version;

    for (const auto& m : materials)
    {
        if (!WriteString(ofs, m.name)) return false;

        // ---- Legacy values ----
        if (!WriteColor4(ofs, m.diffuseColor)) return false;
        if (!WriteColor4(ofs, m.specularColor)) return false;
        if (!WriteColor4(ofs, m.ambientColor)) return false;
        if (!WriteColor4(ofs, m.emissiveColor)) return false;

        if (!WriteF32(ofs, m.shininess)) return false;
        if (!WriteF32(ofs, m.opacity)) return false;
        if (!WriteF32(ofs, m.specularStrength)) return false;

        if (!WriteBool(ofs, m.twoSided)) return false;

        // legacy has flags...
        if (!WriteBool(ofs, m.hasDiffuseColor)) return false;
        if (!WriteBool(ofs, m.hasSpecularColor)) return false;
        if (!WriteBool(ofs, m.hasAmbientColor)) return false;
        if (!WriteBool(ofs, m.hasEmissiveColor)) return false;

        if (!WriteBool(ofs, m.hasShininess)) return false;
        if (!WriteBool(ofs, m.hasOpacity)) return false;
        if (!WriteBool(ofs, m.hasSpecularStrength)) return false;
        if (!WriteBool(ofs, m.hasTwoSided)) return false;

        if (!WriteBool(ofs, m.hasLegacyDiffuseTex)) return false;
        if (!WriteBool(ofs, m.hasLegacySpecularTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyAmbientTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyEmissiveTex)) return false;

        if (!WriteBool(ofs, m.hasLegacyHeightTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyShinessTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyOpacityTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyDisplaceTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyLightmapTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyReflectionTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyBasecolorTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyNormalcameraTex)) return false;
        if (!WriteBool(ofs, m.hasLegacyEmissionTex)) return false;

        if (!WriteBool(ofs, m.hasAnyLegacy)) return false;

        // ★ legacy textures（ver付きで1回だけ）
        if (!WriteTextureRef(ofs, m.diffuseTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.specularTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.ambientTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.emissiveTexLegacy, ver)) return false;

        if (!WriteTextureRef(ofs, m.heightTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.shinessTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.opacityTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.displaceTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.lightmapTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.reflectionTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.basecolorTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.normalcameraTexLegacy, ver)) return false;
        if (!WriteTextureRef(ofs, m.emissionTexLegacy, ver)) return false;

        // ---- PBR textures ----（ver付きで1回だけ）
        if (!WriteTextureRef(ofs, m.baseColorTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.normalTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.metallicTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.roughnessTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.occlusionTex, ver)) return false;
        if (!WriteTextureRef(ofs, m.emissiveTex, ver)) return false;

        // pbr texture flags
        if (!WriteBool(ofs, m.hasPbrBaseColorTex)) return false;
        if (!WriteBool(ofs, m.hasPbrNormalTex)) return false;
        if (!WriteBool(ofs, m.hasPbrMetallicTex)) return false;
        if (!WriteBool(ofs, m.hasPbrRoughnessTex)) return false;
        if (!WriteBool(ofs, m.hasPbrOcclusionTex)) return false;
        if (!WriteBool(ofs, m.hasPbrEmissiveTex)) return false;

        // pbr factors
        if (!WriteColor4(ofs, m.baseColorFactor)) return false;
        if (!WriteF32(ofs, m.metallicFactor)) return false;
        if (!WriteF32(ofs, m.roughnessFactor)) return false;

        if (!WriteBool(ofs, m.hasBaseColorFactor)) return false;
        if (!WriteBool(ofs, m.hasMetallicFactor)) return false;
        if (!WriteBool(ofs, m.hasRoughnessFactor)) return false;

        if (!WriteColor4(ofs, m.emissiveFactor)) return false;
        if (!WriteBool(ofs, m.hasEmissiveFactor)) return false;

        if (!WriteBool(ofs, m.hasAnyPBR)) return false;
    }

    return true;
}

// ==========================
// Reader
// ==========================
bool SceneMaterialBinaryReader::Read(const std::filesystem::path& path,
    std::vector<MaterialData>& outMaterials) const
{
    outMaterials.clear();

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    MaterialBinHeader h{};
    ifs.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!ifs) return false;

    if (!IsMagicOk(h.magic)) return false;
    if (h.version != 2u) return false;

    constexpr std::uint32_t kMaxMaterials = 1u << 20;
    if (h.materialCount > kMaxMaterials) return false;

    outMaterials.resize(h.materialCount);

    const std::uint32_t ver = h.version;

    for (std::uint32_t i = 0; i < h.materialCount; ++i)
    {
        auto& m = outMaterials[i];

        if (!ReadString(ifs, m.name)) return false;

        // legacy values
        if (!ReadColor4(ifs, m.diffuseColor)) return false;
        if (!ReadColor4(ifs, m.specularColor)) return false;
        if (!ReadColor4(ifs, m.ambientColor)) return false;
        if (!ReadColor4(ifs, m.emissiveColor)) return false;

        if (!ReadF32(ifs, m.shininess)) return false;
        if (!ReadF32(ifs, m.opacity)) return false;
        if (!ReadF32(ifs, m.specularStrength)) return false;

        if (!ReadBool(ifs, m.twoSided)) return false;

        // legacy has flags...
        if (!ReadBool(ifs, m.hasDiffuseColor)) return false;
        if (!ReadBool(ifs, m.hasSpecularColor)) return false;
        if (!ReadBool(ifs, m.hasAmbientColor)) return false;
        if (!ReadBool(ifs, m.hasEmissiveColor)) return false;

        if (!ReadBool(ifs, m.hasShininess)) return false;
        if (!ReadBool(ifs, m.hasOpacity)) return false;
        if (!ReadBool(ifs, m.hasSpecularStrength)) return false;
        if (!ReadBool(ifs, m.hasTwoSided)) return false;

        if (!ReadBool(ifs, m.hasLegacyDiffuseTex)) return false;
        if (!ReadBool(ifs, m.hasLegacySpecularTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyAmbientTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyEmissiveTex)) return false;

        if (!ReadBool(ifs, m.hasLegacyHeightTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyShinessTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyOpacityTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyDisplaceTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyLightmapTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyReflectionTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyBasecolorTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyNormalcameraTex)) return false;
        if (!ReadBool(ifs, m.hasLegacyEmissionTex)) return false;

        if (!ReadBool(ifs, m.hasAnyLegacy)) return false;

        // ★ legacy textures（ver付きで1回だけ）
        if (!ReadTextureRef(ifs, m.diffuseTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.specularTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.ambientTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.emissiveTexLegacy, ver)) return false;

        if (!ReadTextureRef(ifs, m.heightTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.shinessTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.opacityTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.displaceTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.lightmapTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.reflectionTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.basecolorTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.normalcameraTexLegacy, ver)) return false;
        if (!ReadTextureRef(ifs, m.emissionTexLegacy, ver)) return false;

        // pbr textures（ver付きで1回だけ）
        if (!ReadTextureRef(ifs, m.baseColorTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.normalTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.metallicTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.roughnessTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.occlusionTex, ver)) return false;
        if (!ReadTextureRef(ifs, m.emissiveTex, ver)) return false;

        if (!ReadBool(ifs, m.hasPbrBaseColorTex)) return false;
        if (!ReadBool(ifs, m.hasPbrNormalTex)) return false;
        if (!ReadBool(ifs, m.hasPbrMetallicTex)) return false;
        if (!ReadBool(ifs, m.hasPbrRoughnessTex)) return false;
        if (!ReadBool(ifs, m.hasPbrOcclusionTex)) return false;
        if (!ReadBool(ifs, m.hasPbrEmissiveTex)) return false;

        if (!ReadColor4(ifs, m.baseColorFactor)) return false;
        if (!ReadF32(ifs, m.metallicFactor)) return false;
        if (!ReadF32(ifs, m.roughnessFactor)) return false;

        if (!ReadBool(ifs, m.hasBaseColorFactor)) return false;
        if (!ReadBool(ifs, m.hasMetallicFactor)) return false;
        if (!ReadBool(ifs, m.hasRoughnessFactor)) return false;

        if (!ReadColor4(ifs, m.emissiveFactor)) return false;
        if (!ReadBool(ifs, m.hasEmissiveFactor)) return false;

        if (!ReadBool(ifs, m.hasAnyPBR)) return false;
    }

    return true;
}

std::vector<const MaterialData*>
SceneMaterialBinaryReader::GetValidPBRMaterials(const std::vector<MaterialData>& materials)
{
    std::vector<const MaterialData*> out;
    out.reserve(materials.size());
    for (const auto& m : materials)
    {
        if (m.hasAnyPBR) out.push_back(&m);
    }
    return out;
}

std::vector<const MaterialData*>
SceneMaterialBinaryReader::GetValidLegacyMaterials(const std::vector<MaterialData>& materials)
{
    std::vector<const MaterialData*> out;
    out.reserve(materials.size());
    for (const auto& m : materials)
    {
        if (m.hasAnyLegacy) out.push_back(&m);
    }
    return out;
}

