#pragma once
// ↑ このヘッダーファイルが同じ .cpp から何回 include されても、1回だけ読み込むための指定

#include <cstdint>       // ↑ std::uint8_t などの「決まったサイズの整数型」を使うため
#include <filesystem>    // ↑ std::filesystem::path（ファイルパス型）などを使うため
#include <fstream>       // ↑ std::ifstream（ファイル読み込み）を使うため
#include <limits>        // ↑ numeric_limits（型の最大値など）を使うため
#include <stdexcept>     // ↑ std::runtime_error（例外）を使うため
#include <string>        // ↑ std::string / std::wstring / std::u8string を使うため
#include <string_view>   // ↑ std::string_view（文字列の参照）を使うため
#include <vector>        // ↑ std::vector（可変長配列）を使うため
#include "../system/utility.h"

#include "EncodingConverter.h"  // ★追加：文字コード変換はクラス側へ分離

namespace utility
{
    // ---- PathFrom... ----  // ↑ ここから「文字列 → filesystem::path」変換

    // 純UTF-8入力として扱う（従来通り）
    inline std::filesystem::path PathFromUtf8(std::string_view utf8)
    {
#if defined(_WIN32)
        // ↑ WindowsではパスがUTF-16に強いので、UTF-16(wstring)からpathを作る
        return std::filesystem::path(EncodingConverter::Utf8ToWide(utf8));
#else
        // ↑ POSIXはパスを「バイト列」として扱うので、UTF-8バイト列をそのままpathにする
        return std::filesystem::path(std::string(utf8));
#endif
    }

    // C++20 u8string_view 入力（純UTF-8）
    inline std::filesystem::path PathFromUtf8(std::u8string_view utf8)
    {
        const char* p = reinterpret_cast<const char*>(utf8.data());
        return PathFromUtf8(std::string_view{ p, utf8.size() });
    }

    // ★追加：Windowsで「UTF-8→失敗したらCP932」フォールバックして path を作る
    inline std::filesystem::path PathFromUtf8OrCp932(std::string_view bytes)
    {
#if defined(_WIN32)
        // 1) まずUTF-8として厳密変換（不正UTF-8なら例外）
        try
        {
            return std::filesystem::path(EncodingConverter::Utf8ToWide(bytes));
        }
        catch (...)
        {
            // 2) 失敗したらCP932(Shift-JIS)として変換
            return std::filesystem::path(EncodingConverter::Cp932ToWide(bytes));
        }
#else
        // POSIXはバイト列。ここでは「UTF-8想定」でそのまま通す
        return std::filesystem::path(std::string(bytes));
#endif
    }

    inline std::filesystem::path PathFromUtf8OrCp932(std::u8string_view bytes)
    {
        const char* p = reinterpret_cast<const char*>(bytes.data());
        return PathFromUtf8OrCp932(std::string_view{ p, bytes.size() });
    }

    // ---- Path -> UTF-8 string ----  // ↑ ここから「filesystem::path → UTF-8文字列」
/*
    inline std::string PathToUtf8String(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        // ↑ path を wstring(UTF-16)として取り出し、UTF-8へ変換
        return EncodingConverter::WideToUtf8(path.wstring());
#else
        // ↑ C++20では u8string() が std::u8string(char8_t) を返す
        std::u8string u8 = path.u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
#endif
    }

*/

    // ---- ReadAllBytes ----  // ↑ ここから「ファイルを丸ごと読み込む」

    inline std::vector<std::uint8_t> ReadAllBytes(const std::filesystem::path& path)
    {
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs)
            throw std::runtime_error("Failed to open file: " + utility::PathToUtf8String(path));

        const auto endPos = ifs.tellg();
        if (endPos < 0)
            throw std::runtime_error("Failed to get file size: " + utility::PathToUtf8String(path));

        const std::uint64_t size64 = static_cast<std::uint64_t>(endPos);
        if (size64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
            throw std::runtime_error("File too large: " + utility::PathToUtf8String(path));

        std::vector<std::uint8_t> data(static_cast<std::size_t>(size64));

        ifs.seekg(0, std::ios::beg);

        if (!ifs.read(reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(data.size())))
            throw std::runtime_error("Failed to read file: " + utility::PathToUtf8String(path));

        return data;
    }

    // 既存：UTF-8として読む（厳密）
    inline std::vector<std::uint8_t> ReadAllBytesUtf8(std::string_view utf8Path)
    {
        return ReadAllBytes(PathFromUtf8(utf8Path));
    }

    inline std::vector<std::uint8_t> ReadAllBytesUtf8(std::u8string_view utf8Path)
    {
        return ReadAllBytes(PathFromUtf8(utf8Path));
    }

    // ★追加：互換性重視（UTF-8→失敗したらCP932）
    inline std::vector<std::uint8_t> ReadAllBytesUtf8OrCp932(std::string_view bytesPath)
    {
        return ReadAllBytes(PathFromUtf8OrCp932(bytesPath));
    }

    inline std::vector<std::uint8_t> ReadAllBytesUtf8OrCp932(std::u8string_view bytesPath)
    {
        return ReadAllBytes(PathFromUtf8OrCp932(bytesPath));
    }

} // namespace utility
