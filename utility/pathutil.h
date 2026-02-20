#pragma once                                                   // 1回だけ読み込む

#include <filesystem>                                           // std::filesystem::path
#include <string>                                               // std::string
#include <string_view>                                          // std::string_view / std::u8string_view

namespace utility                                               // utility 名前空間
{
    struct PathUtil                                              // path関連ユーティリティ
    {
        static std::filesystem::path FromUtf8(std::string_view);        // UTF-8 -> path
        static std::filesystem::path FromUtf8(std::u8string_view);      // char8_t版

        static std::filesystem::path FromUtf8OrCp932(std::string_view); // UTF-8優先/失敗ならCP932
        static std::filesystem::path FromUtf8OrCp932(std::u8string_view); // char8_t版

        static std::string ToUtf8Filename(const std::filesystem::path&); // filenameだけ
        static std::string ToUtf8Full(const std::filesystem::path&);     // フルパス
    };

    // 互換ラッパ：既存名を残す（呼び出し側の修正を最小化）
    inline std::filesystem::path PathFromUtf8(std::string_view s) { return PathUtil::FromUtf8(s); }                 // 互換
    inline std::filesystem::path PathFromUtf8(std::u8string_view s) { return PathUtil::FromUtf8(s); }               // 互換
    inline std::filesystem::path PathFromUtf8OrCp932(std::string_view s) { return PathUtil::FromUtf8OrCp932(s); }   // 互換
    inline std::filesystem::path PathFromUtf8OrCp932(std::u8string_view s) { return PathUtil::FromUtf8OrCp932(s); } // 互換
//    inline std::string PathToUtf8String(const std::filesystem::path& p) { return PathUtil::ToUtf8Filename(p); }     // 互換
//   inline std::string PathToUtf8StringFull(const std::filesystem::path& p) { return PathUtil::ToUtf8Full(p); }     // 互換
}                                                               // namespace utility 終わり
