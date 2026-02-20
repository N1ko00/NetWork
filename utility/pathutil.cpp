#include "PathUtil.h"                                           // 宣言

#if defined(_WIN32)                                             // Windows のとき
#include "EncodingConverter.h"                                  // 文字列変換を利用
#endif                                                          // Windows ここまで

namespace utility                                               // utility 名前空間
{
    std::filesystem::path PathUtil::FromUtf8(std::string_view utf8) // UTF-8 -> path
    {
#if defined(_WIN32)                                             // Windows
        return std::filesystem::path(                            // pathを生成
            EncodingConverter::Utf8ToWide(utf8));                // UTF-8->UTF-16 して渡す
#else                                                           // POSIX
        return std::filesystem::path(                            // pathを生成
            std::string(utf8));                                  // UTF-8バイト列をそのまま渡す
#endif                                                          // 分岐終わり
    }

    std::filesystem::path PathUtil::FromUtf8(std::u8string_view utf8) // char8_t版
    {
        const char* p = reinterpret_cast<const char*>(utf8.data());  // char8_t* -> char*
        return FromUtf8(std::string_view{ p, utf8.size() });         // string_viewにして委譲
    }

    std::filesystem::path PathUtil::FromUtf8OrCp932(std::string_view bytes) // UTF-8優先/失敗CP932
    {
#if defined(_WIN32)                                             // Windows
        try                                                     // 例外で判定
        {
            return std::filesystem::path(                        // path生成
                EncodingConverter::Utf8ToWide(bytes));           // UTF-8厳密変換
        }
        catch (...)                                             // UTF-8が不正など
        {
            return std::filesystem::path(                        // path生成
                EncodingConverter::Cp932ToWide(bytes));          // CP932で変換
        }
#else                                                           // POSIX
        return std::filesystem::path(                            // path生成
            std::string(bytes));                                 // バイト列として通す
#endif                                                          // 分岐終わり
    }

    std::filesystem::path PathUtil::FromUtf8OrCp932(std::u8string_view bytes) // char8_t版
    {
        const char* p = reinterpret_cast<const char*>(bytes.data()); // char8_t* -> char*
        return FromUtf8OrCp932(std::string_view{ p, bytes.size() });  // 委譲
    }

    std::string PathUtil::ToUtf8Filename(const std::filesystem::path& p) // filenameだけUTF-8
    {
#if defined(_WIN32)                                             // Windows
        return EncodingConverter::WideToUtf8(                    // UTF-16->UTF-8
            p.filename().wstring());                             // filenameをUTF-16として取得
#else                                                           // POSIX
        const std::u8string u8 = p.filename().u8string();        // char8_t のUTF-8を取得
        return std::string(                                      // std::stringへ詰め替え
            reinterpret_cast<const char*>(u8.data()),            // バイト列として扱う
            u8.size());                                          // サイズ
#endif                                                          // 分岐終わり
    }

    std::string PathUtil::ToUtf8Full(const std::filesystem::path& p) // フルパスUTF-8
    {
#if defined(_WIN32)                                             // Windows
        return EncodingConverter::WideToUtf8(                    // UTF-16->UTF-8
            p.wstring());                                        // フルパスUTF-16を取得
#else                                                           // POSIX
        const std::u8string u8 = p.u8string();                   // フルパスをUTF-8で取得
        return std::string(                                      // std::stringへ詰め替え
            reinterpret_cast<const char*>(u8.data()),            // バイト列
            u8.size());                                          // サイズ
#endif                                                          // 分岐終わり
    }
}                                                               // namespace utility 終わり
