#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>

#include "../../system/CTexture.h"

class TextureArray : NonCopyable
{
public:
    enum class TextureType {
        diffuse,
        specular,
        normal
    };

    static constexpr std::size_t kCount = 3;

public:
    TextureArray() = default;

    /// @brief 指定スロットのテクスチャ参照
    CTexture& At(TextureType type);
    const CTexture& At(TextureType type) const;

    /// @brief 指定スロットが有効か
    bool Has(TextureType type) const;

    /// @brief 有効フラグをセット（ロード完了時などに）
    void MarkUsed(TextureType type, bool v = true);

    /// @brief 指定スロットをクリア
    void Clear(TextureType type);

    /// @brief 全スロットをクリア
    void ClearAll();

private:
    std::size_t ToIndex(TextureType type) const;

    std::array<CTexture, kCount> m_textures{};
    std::array<bool, kCount> m_used{}; // falseで初期化される
};
