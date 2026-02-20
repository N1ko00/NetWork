#include "EncodingConverter.h"                                // 宣言

#if defined(_WIN32)                                           // Windows のときだけ実装

namespace utility                                             // utility 名前空間
{
    static std::wstring FormatMessageWFromError(DWORD err)     // エラー文(UTF-16)取得
    {
        if (err == 0) return L"";                             // エラー無しなら空

        LPWSTR buf = nullptr;                                 // OSが確保するバッファ
        const DWORD flags =                                   // FormatMessage フラグ
            FORMAT_MESSAGE_ALLOCATE_BUFFER |                   // OSにバッファ確保させる
            FORMAT_MESSAGE_FROM_SYSTEM |                       // システムメッセージから
            FORMAT_MESSAGE_IGNORE_INSERTS;                     // 置換を無視

        const DWORD len = ::FormatMessageW(                    // 取得実行
            flags,                                             // フラグ
            nullptr,                                           // ソース
            err,                                               // エラーコード
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),         // 言語
            reinterpret_cast<LPWSTR>(&buf),                    // 出力バッファ
            0,                                                 // サイズ(ALLOCATE使用)
            nullptr);                                          // 引数

        std::wstring msg;                                     // 返すメッセージ
        if (len && buf)                                       // 取得成功
        {
            msg.assign(buf, buf + len);                       // wstringにコピー
            ::LocalFree(buf);                                 // OSバッファ解放
        }
        return msg;                                           // 返す
    }

    std::string EncodingConverter::GetLastErrorMessageUtf8(DWORD err) // UTF-8化して返す
    {
        const std::wstring w = FormatMessageWFromError(err);   // UTF-16 取得
        if (w.empty()) return {};                              // 空なら空
        return WideToUtf8(std::wstring_view{ w });              // UTF-8へ変換
    }

    std::wstring EncodingConverter::MultiByteToWide(           // 任意CP -> UTF-16
        UINT codePage,                                         // 入力コードページ
        std::string_view bytes,                                // 入力バイト列
        DWORD flags)                                           // 変換フラグ
    {
        if (bytes.empty()) return {};                          // 空なら空

        const int srcLen = static_cast<int>(bytes.size());     // WinAPIはint長

        ::SetLastError(0);                                     // エラー初期化
        const int needed = ::MultiByteToWideChar(              // 必要数を問い合わせ
            codePage,                                          // コードページ
            flags,                                             // フラグ
            bytes.data(),                                      // 入力
            srcLen,                                            // 入力長
            nullptr,                                           // 出力なし
            0);                                                // 出力長0

        if (needed <= 0)                                       // 失敗
        {
            const DWORD e = ::GetLastError();                  // エラー取得
            throw std::runtime_error(                          // 例外
                "MultiByteToWideChar failed. " + GetLastErrorMessageUtf8(e));
        }

        std::wstring w(static_cast<size_t>(needed), L'\0');    // 出力バッファ確保

        ::SetLastError(0);                                     // エラー初期化
        const int written = ::MultiByteToWideChar(             // 実変換
            codePage,                                          // コードページ
            flags,                                             // フラグ
            bytes.data(),                                      // 入力
            srcLen,                                            // 入力長
            w.data(),                                          // 出力先
            needed);                                           // 出力長

        if (written != needed)                                 // 想定と違う
        {
            const DWORD e = ::GetLastError();                  // エラー取得
            throw std::runtime_error(                          // 例外
                "MultiByteToWideChar failed (size mismatch). " + GetLastErrorMessageUtf8(e));
        }

        return w;                                              // UTF-16を返す
    }

    std::wstring EncodingConverter::Utf8ToWide(std::string_view utf8) // UTF-8 -> UTF-16
    {
        return MultiByteToWide(CP_UTF8, utf8, MB_ERR_INVALID_CHARS);  // 不正UTF-8はエラー
    }

    std::wstring EncodingConverter::Cp932ToWide(std::string_view sjis) // CP932 -> UTF-16
    {
        return MultiByteToWide(932u, sjis, 0);                 // CP932は互換重視でflags=0
    }

    std::string EncodingConverter::WideToUtf8(std::wstring_view w) // UTF-16 -> UTF-8
    {
        if (w.empty()) return {};                               // 空なら空

        const int srcLen = static_cast<int>(w.size());          // WinAPIはint長

        ::SetLastError(0);                                      // エラー初期化
        const int needed = ::WideCharToMultiByte(               // 必要バイト数
            CP_UTF8,                                            // UTF-8
            0,                                                  // フラグ
            w.data(),                                           // 入力
            srcLen,                                             // 入力長
            nullptr,                                            // 出力なし
            0,                                                  // 出力長0
            nullptr,                                            // 代替
            nullptr);                                           // 代替フラグ

        if (needed <= 0)                                        // 失敗
        {
            const DWORD e = ::GetLastError();                   // エラー取得
            throw std::runtime_error(                           // 例外
                "WideCharToMultiByte failed. " + GetLastErrorMessageUtf8(e));
        }

        std::string s(static_cast<size_t>(needed), '\0');       // 出力確保

        ::SetLastError(0);                                      // エラー初期化
        const int written = ::WideCharToMultiByte(              // 実変換
            CP_UTF8,                                            // UTF-8
            0,                                                  // フラグ
            w.data(),                                           // 入力
            srcLen,                                             // 入力長
            s.data(),                                           // 出力
            needed,                                             // 出力長
            nullptr,                                            // 代替
            nullptr);                                           // 代替フラグ

        if (written != needed)                                  // 想定と違う
        {
            const DWORD e = ::GetLastError();                   // エラー取得
            throw std::runtime_error(                           // 例外
                "WideCharToMultiByte failed (size mismatch). " + GetLastErrorMessageUtf8(e));
        }

        return s;                                               // UTF-8を返す
    }
}                                                              // namespace utility 終わり

#endif                                                         // _WIN32 終わり
