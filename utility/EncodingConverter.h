#pragma once                                                // 1回だけ読み込む

#include <string>                                            // std::string / std::wstring
#include <string_view>                                       // std::string_view / std::wstring_view
#include <stdexcept>                                         // std::runtime_error

#if defined(_WIN32)                                          // Windows のときだけ
#include <Windows.h>                                         // WinAPI（変換/エラー文字列）
#endif                                                       // Windows ここまで

namespace utility                                            // utility 名前空間
{
    class EncodingConverter                                  // 文字コード変換クラス
    {
    public:                                                  // 公開
#if defined(_WIN32)                                          // Windows 実装

        static std::wstring MultiByteToWide(                 // 任意CP -> UTF-16
            UINT codePage,                                   // 入力のコードページ
            std::string_view bytes,                           // 入力バイト列
            DWORD flags);                                     // 変換フラグ

        static std::wstring Utf8ToWide(                       // UTF-8 -> UTF-16
            std::string_view utf8);                           // UTF-8 バイト列

        static std::wstring Cp932ToWide(                      // CP932 -> UTF-16
            std::string_view sjis);                           // SJIS/CP932 バイト列

        static std::string WideToUtf8(                        // UTF-16 -> UTF-8
            std::wstring_view w);                             // UTF-16 文字列ビュー

        static std::string WideToUtf8(                        // 便利オーバーロード
            const std::wstring& w)                            // UTF-16 文字列
        {                                                     // 本体
            return WideToUtf8(std::wstring_view{ w });         // viewへ渡す
        }                                                     // 終わり

        static std::string GetLastErrorMessageUtf8(            // Windowsエラー文字列(UTF-8)
            DWORD err = ::GetLastError());                     // 省略時はGetLastError()

#else                                                        // 非Windows

        static std::wstring MultiByteToWide(                   // 非Windowsでは未提供
            unsigned, std::string_view, unsigned long)         // 引数だけ揃える
        {                                                     // 本体
            throw std::runtime_error(                          // 例外で通知
                "EncodingConverter: MultiByteToWide is not supported on this platform.");
        }                                                     // 終わり

        static std::wstring Utf8ToWide(std::string_view)       // 非Windowsでは未提供
        {                                                     // 本体
            throw std::runtime_error(                          // 例外で通知
                "EncodingConverter: Utf8ToWide is not supported on this platform.");
        }                                                     // 終わり

        static std::wstring Cp932ToWide(std::string_view)      // 非Windowsでは未提供
        {                                                     // 本体
            throw std::runtime_error(                          // 例外で通知
                "EncodingConverter: Cp932ToWide is not supported on this platform.");
        }                                                     // 終わり

        static std::string WideToUtf8(std::wstring_view)       // 非Windowsでは未提供
        {                                                     // 本体
            throw std::runtime_error(                          // 例外で通知
                "EncodingConverter: WideToUtf8 is not supported on this platform.");
        }                                                     // 終わり

        static std::string GetLastErrorMessageUtf8(            // 非Windowsでは空文字
            unsigned long = 0)                                 // 引数互換
        {                                                     // 本体
            return {};                                         // 空を返す
        }                                                     // 終わり

#endif                                                       // プラットフォーム分岐終わり
    };                                                        // class 終わり
}                                                             // namespace utility 終わり
