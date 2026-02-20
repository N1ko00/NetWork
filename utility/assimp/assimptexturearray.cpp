#include "AssimpTextureArray.h"

std::size_t TextureArray::ToIndex(TextureType type)const
{
    const auto idx = static_cast<std::size_t>(type);
    if (idx >= kCount)
    {
        throw std::out_of_range("TextureType is out of range for TextureArray.");
    }
    return idx;
}

CTexture& TextureArray::At(TextureType type)
{
    return m_textures[ToIndex(type)];
}

const CTexture& TextureArray::At(TextureType type) const
{
    return m_textures[ToIndex(type)];
}

bool TextureArray::Has(TextureType type) const
{
    return m_used[ToIndex(type)];
}

void TextureArray::MarkUsed(TextureType type, bool v)
{
    m_used[ToIndex(type)] = v;
}

void TextureArray::Clear(TextureType type)
{
    const auto idx = ToIndex(type);
    m_textures[idx].Reset();
    m_used[idx] = false;
}

void TextureArray::ClearAll()
{
    for (std::size_t i = 0; i < kCount; ++i)
    {
        m_textures[i].Reset();
        m_used[i] = false;
    }
}
