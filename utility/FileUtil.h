#pragma once
// ↑ 何回 include されても1回だけ読み込む

#include <cstdint>       // std::uint8_t
#include <filesystem>    // std::filesystem::path
#include <string_view>   // std::string_view / std::u8string_view
#include <vector>        // std::vector

namespace utility
{
    // ============================================================
    // FileUtil:
    //   - ファイルを丸ごと読み込むユーティリティ
    //   - パス文字列→path は PathUtil を利用
    //   - エラーメッセージ用の path→UTF-8 も PathUtil を利用
    // ============================================================
    class FileUtil
    {
    public:
        // path 指定で全読み込み
        static std::vector<std::uint8_t> ReadAllBytes(const std::filesystem::path& path);

        // 入力パスが UTF-8 確定
        static std::vector<std::uint8_t> ReadAllBytesUtf8(std::string_view utf8Path);
        static std::vector<std::uint8_t> ReadAllBytesUtf8(std::u8string_view utf8Path);

        // 入力パスが UTF-8 か CP932 か不明（Windowsでは UTF-8優先→失敗CP932）
        static std::vector<std::uint8_t> ReadAllBytesUtf8OrCp932(std::string_view bytesPath);
        static std::vector<std::uint8_t> ReadAllBytesUtf8OrCp932(std::u8string_view bytesPath);
    };

    // ============================================================
    // 互換ラッパ（旧 API 名を残したい場合だけ使う）
    // ============================================================
    inline std::vector<std::uint8_t> ReadAllBytes(const std::filesystem::path& path)
    {
        return FileUtil::ReadAllBytes(path);
    }

    inline std::vector<std::uint8_t> ReadAllBytesUtf8(std::string_view utf8Path)
    {
        return FileUtil::ReadAllBytesUtf8(utf8Path);
    }

    inline std::vector<std::uint8_t> ReadAllBytesUtf8(std::u8string_view utf8Path)
    {
        return FileUtil::ReadAllBytesUtf8(utf8Path);
    }

    inline std::vector<std::uint8_t> ReadAllBytesUtf8OrCp932(std::string_view bytesPath)
    {
        return FileUtil::ReadAllBytesUtf8OrCp932(bytesPath);
    }

    inline std::vector<std::uint8_t> ReadAllBytesUtf8OrCp932(std::u8string_view bytesPath)
    {
        return FileUtil::ReadAllBytesUtf8OrCp932(bytesPath);
    }

} // namespace utility
