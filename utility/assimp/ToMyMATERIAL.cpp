#include "ToMyMATERIAL.h"

// MaterialData::Color4 -> Color
static Color ToColor(const AssimpSceneMeshExtractor::MaterialData::Color4& c) noexcept
{
    return { c.r, c.g, c.b, c.a };
}

static Color DefaultAmbient()  noexcept { return { 0, 0, 0, 1 }; }
static Color DefaultDiffuse()  noexcept { return { 1, 1, 1, 1 }; }
static Color DefaultSpecular() noexcept { return { 1, 1, 1, 1 }; }
static Color DefaultEmission() noexcept { return { 0, 0, 0, 1 }; }

// 「何らかのテクスチャを持っているか？」（Legacy + PBR 両対応）
static bool HasAnyTexture(const AssimpSceneMeshExtractor::MaterialData& m) noexcept
{
    auto has = [](const AssimpSceneMeshExtractor::MaterialData::TextureRef& t) noexcept
        {
            return t.HasTexture();
        };

    // Legacy slots
    if (has(m.diffuseTex)) return true;
    if (has(m.specularTex)) return true;
    if (has(m.ambientTex)) return true;
    if (has(m.emissiveTexLegacy)) return true;

    if (has(m.heightTexLegacy)) return true;
    if (has(m.shinessTexLegacy)) return true;
    if (has(m.opacityTexLegacy)) return true;
    if (has(m.displaceTexLegacy)) return true;
    if (has(m.lightmapTexLegacy)) return true;
    if (has(m.reflectionTexLegacy)) return true;
    if (has(m.basecolorTexLegacy)) return true;
    if (has(m.normalcameraTexLegacy)) return true;
    if (has(m.emissionTexLegacy)) return true;

    // PBR slots
    if (has(m.baseColorTex)) return true;
    if (has(m.normalTex)) return true;
    if (has(m.metallicTex)) return true;
    if (has(m.roughnessTex)) return true;
    if (has(m.occlusionTex)) return true;
    if (has(m.emissiveTex)) return true;

    return false;
}

// ディフューズテクスチャを持っているか？」
static bool HasDiffuseTexture(const AssimpSceneMeshExtractor::MaterialData& m) noexcept
{
    auto has = [](const AssimpSceneMeshExtractor::MaterialData::TextureRef& t) noexcept
        {
            return t.HasTexture();
        };

    // Legacy slots
    if (has(m.diffuseTex)) return true;

    return false;
}

// スペキュラテクスチャを持っているか？」
static bool HaspecularTexture(const AssimpSceneMeshExtractor::MaterialData& m) noexcept
{
    auto has = [](const AssimpSceneMeshExtractor::MaterialData::TextureRef& t) noexcept
        {
            return t.HasTexture();
        };

    // Legacy slots
    if (has(m.specularTex)) return true;

    return false;
}

// AMBIENTテクスチャを持っているか？」
static bool HasAmbientTexture(const AssimpSceneMeshExtractor::MaterialData& m) noexcept
{
    auto has = [](const AssimpSceneMeshExtractor::MaterialData::TextureRef& t) noexcept
        {
            return t.HasTexture();
        };

    // Legacy slots
    if (has(m.ambientTex)) return true;

    return false;
}

// EMMISIVEテクスチャを持っているか？」
static bool HasEmissiveTexture(const AssimpSceneMeshExtractor::MaterialData& m) noexcept
{
    auto has = [](const AssimpSceneMeshExtractor::MaterialData::TextureRef& t) noexcept
        {
            return t.HasTexture();
        };

    // Legacy slots
    if (has(m.emissiveTex)) return true;

    return false;
}



static MATERIAL ConvertToLegacyMaterial(const AssimpSceneMeshExtractor::MaterialData& src) noexcept
{
    MATERIAL dst{};

    // ---- Colors（hasフラグが false の時は既定値）----
    dst.Ambient = src.hasAmbientColor ? ToColor(src.ambientColor) : DefaultAmbient();
    dst.Diffuse = src.hasDiffuseColor ? ToColor(src.diffuseColor) : DefaultDiffuse();
    dst.Specular = src.hasSpecularColor ? ToColor(src.specularColor) : DefaultSpecular();
    dst.Emission = src.hasEmissiveColor ? ToColor(src.emissiveColor) : DefaultEmission();

    // ---- Opacity を Diffuse.a に反映（必要なら）----
    // 「旧来MATERIALにopacity欄が無い」ので、alphaとして持たせるのが扱いやすいです。
    if (src.hasOpacity)
    {
        dst.Diffuse.w = std::clamp(src.opacity, 0.0f, 1.0f);
    }

    // ---- Shininess ----
    dst.Shiness = src.hasShininess ? std::max(0.0f, src.shininess) : 0.0f;

    // ---- TextureEnable ----
    dst.TextureEnable = HasDiffuseTexture(src) ? TRUE : FALSE;

    return dst;
}

std::vector<MATERIAL> BuildLegacyMaterialVector(
    const std::vector<AssimpSceneMeshExtractor::MaterialData>& mtrls)
{
    std::vector<MATERIAL> out;
    out.reserve(mtrls.size());

    for (const auto& m : mtrls)
    {
        out.push_back(ConvertToLegacyMaterial(m));
    }
    return out;
}

