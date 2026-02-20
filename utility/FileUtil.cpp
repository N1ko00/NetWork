#include "FileUtil.h"

#include <fstream>       // std::ifstream
#include <limits>        // std::numeric_limits
#include <stdexcept>     // std::runtime_error

#include "PathUtil.h"    // ★ PathUtil を利用（path変換・UTF-8文字列化）

namespace utility
{
    std::vector<std::uint8_t> FileUtil::ReadAllBytes(const std::filesystem::path& path)
    {
        // バイナリ＋末尾へ（サイズ取得のため）
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);

        if (!ifs)
        {
            // 例外メッセージは UTF-8 にして見やすくする（PathUtil 利用）
            throw std::runtime_error("Failed to open file: " + PathUtil::ToUtf8Full(path));
        }

        const auto endPos = ifs.tellg();
        if (endPos < 0)
        {
            throw std::runtime_error("Failed to get file size: " + PathUtil::ToUtf8Full(path));
        }

        const std::uint64_t size64 = static_cast<std::uint64_t>(endPos);

        // vector が確保できる範囲かチェック
        if (size64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::runtime_error("File too large: " + PathUtil::ToUtf8Full(path));
        }

        std::vector<std::uint8_t> data(static_cast<std::size_t>(size64));

        // 先頭へ戻して読み込み
        ifs.seekg(0, std::ios::beg);

        if (!ifs.read(reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(data.size())))
        {
            throw std::runtime_error("Failed to read file: " + PathUtil::ToUtf8Full(path));
        }

        return data;
    }

    std::vector<std::uint8_t> FileUtil::ReadAllBytesUtf8(std::string_view utf8Path)
    {
        // UTF-8文字列 → path は PathUtil に任せる
        return ReadAllBytes(PathUtil::FromUtf8(utf8Path));
    }

    std::vector<std::uint8_t> FileUtil::ReadAllBytesUtf8(std::u8string_view utf8Path)
    {
        return ReadAllBytes(PathUtil::FromUtf8(utf8Path));
    }

    std::vector<std::uint8_t> FileUtil::ReadAllBytesUtf8OrCp932(std::string_view bytesPath)
    {
        // Windowsなら UTF-8厳密→失敗CP932、POSIXならそのまま
        return ReadAllBytes(PathUtil::FromUtf8OrCp932(bytesPath));
    }

    std::vector<std::uint8_t> FileUtil::ReadAllBytesUtf8OrCp932(std::u8string_view bytesPath)
    {
        return ReadAllBytes(PathUtil::FromUtf8OrCp932(bytesPath));
    }

} // namespace utility
