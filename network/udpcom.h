#pragma once

//============================================================
// Windows + Winsock を使うための準備
//============================================================

// Windows.h の巨大な include を抑えてビルドを速くする
#define WIN32_LEAN_AND_MEAN

// windows.h が winsock.h（古い方）を勝手に入れるのを防ぐ
// winsock.h と winsock2.h を両方入れると衝突しやすい
#define _WINSOCKAPI_

#include <winsock2.h>  // Winsock2（socket / sendto / recvfrom など）
#include <ws2tcpip.h>  // inet_pton など（IP文字列変換）

#include <cstdint>
#include <string>

// ws2_32.lib をリンクする（Winsockのライブラリ）
// ※プロジェクト設定で追加しているなら不要
#pragma comment(lib, "ws2_32.lib")

//============================================================
// UDPCOM
// 目的：UDP通信を「最小限の手順」で扱える薄いラッパ
//
// 基本的な使い方（典型）:
//   UDPCOM udp;
//   udp.Open();
//   udp.Bind(12345);                 // 受信するなら必須（送信だけなら不要な場合もある）
//   udp.SetRemote("127.0.0.1", 9999);// Send() の宛先を登録
//   udp.SetNonBlocking(true);        // ポーリングしたい場合
//   udp.Send(...);
//   udp.Recv(...);
//
// ※UDPは「到達保証がない」「順序保証がない」点に注意
//============================================================
class UDPCOM {
public:
    // 生成しただけでは何もしない（Open() で初めて初期化される）
    UDPCOM() = default;

    // 破棄時に自動で Close() する（ソケットとWSAの後片付け）
    ~UDPCOM() { Close(); }

    //========================================================
    // コピー禁止（ソケットは「所有権」を複製できないため）
    //========================================================
    UDPCOM(const UDPCOM&) = delete;
    UDPCOM& operator=(const UDPCOM&) = delete;

    //========================================================
    // ムーブは許可（所有権を移動できる）
    // 例：std::vector<UDPCOM> に入れる、戻り値で返す等が可能
    //========================================================
    UDPCOM(UDPCOM&& rhs) noexcept { MoveFrom(std::move(rhs)); }
    UDPCOM& operator=(UDPCOM&& rhs) noexcept {
        // 自分自身への代入ではないことを確認して
        // いま持ってる資源を閉じた上で、rhsの資源を奪う
        if (this != &rhs) { Close(); MoveFrom(std::move(rhs)); }
        return *this;
    }

    //========================================================
    // --- lifecycle（初期化/設定/終了） ---
    //========================================================

    // Open:
    // 1) WSAStartup で Winsock を初期化
    // 2) UDPソケットを作成
    //
    // 成功: true
    // 失敗: false（outErr に理由文字列が入る）
    bool Open(std::string* outErr = nullptr);

    // Bind:
    // 自分が受信したいポート番号を OS に予約してもらう
    //
    // usePort=0 の場合:
    //   OSが空いているポートを自動割り当てする
    //   （実際のポート番号は BoundPort() で取得できる）
    bool Bind(uint16_t usePort, std::string* outErr = nullptr);

    // SetRemote:
    // Send() で送る相手（IP/ポート）を保持する
    // ※UDPに connect は必須ではないが、「毎回sendtoの宛先を書く」のを省略する目的
    bool SetRemote(const char* ip, uint16_t port, std::string* outErr = nullptr);

    // SetBroadcast:
    // ブロードキャスト送信を許可する/しない
    // 例：255.255.255.255 や サブネットブロードキャストへ送る場合に必要
    bool SetBroadcast(bool enable, std::string* outErr = nullptr);

    // DisableUdpConnReset:
    // Windows 特有の「UDPでICMP Port Unreachable を受けると recvfrom が WSAECONNRESET で失敗する」
    // という挙動を抑止するための設定（よく使われる対策）
    //
    // 特に、送信先が存在しない/一時的に落ちる可能性がある環境で有効
    bool DisableUdpConnReset(std::string* outErr = nullptr);

    // SetNonBlocking:
    // ノンブロッキングのON/OFF
    //
    // enable=true の場合:
    //   受信データが無ければ recvfrom は即座に戻り、WSAEWOULDBLOCK になる
    // enable=false の場合:
    //   recvfrom はデータが来るまで待つ（スレッドが止まる）
    bool SetNonBlocking(bool enable, std::string* outErr = nullptr);

    // Close:
    // ソケットを閉じ、WSACleanup で Winsock を後片付けする
    // 何度呼んでも壊れにくいように実装するのが基本
    void Close();

    //========================================================
    // --- send/recv（送受信） ---
    //========================================================

    // Send:
    // SetRemote() で登録した相手へ UDP 送信する
    //
    // 戻り値:
    //  成功: 送ったバイト数
    //  失敗: SOCKET_ERROR（outErr に理由）
    int Send(const void* data, size_t size, std::string* outErr = nullptr);

    // 宛先を指定して送る（複数相手対応の要）
    int SendTo(const void* data, size_t size, const sockaddr_in& to, std::string* outErr = nullptr);

    // Recv:
    // UDP 受信する
    //
    // outFrom:
    //  送信元IP/ポートが欲しい場合に受け取る（不要なら nullptr でもOK）
    //
    // 戻り値:
    //  成功: 受信したバイト数
    //  失敗: SOCKET_ERROR（ノンブロッキングでデータ無しの場合は WSAEWOULDBLOCK になりがち）
    int Recv(void* buf, size_t size, sockaddr_in* outFrom, std::string* outErr = nullptr);

    // RemoteAddr:
    // SetRemote() で保持している送信先情報を返す
    // （デバッグ表示や、sendto 先の確認に使える）
    const sockaddr_in& RemoteAddr() const { return m_remote; }

    // BoundPort:
    // 現在 bind されているポート番号を返す
    // bind(0) で OS任せにした時に「何番になった？」を知れる
    uint16_t BoundPort() const;

    // NativeSocket:
    // 生の SOCKET を取り出す
    // select / WSAEventSelect / IOCP などを自分でやりたい場合に利用
    SOCKET NativeSocket() const { return m_sock; }

private:
    //========================================================
    // 内部状態（Winsock + ソケット + アドレス情報）
    //========================================================
    WSADATA m_wsa{};                  // WSAStartup の結果が入る
    SOCKET  m_sock{ INVALID_SOCKET }; // UDPソケット本体（未作成なら INVALID_SOCKET）

    sockaddr_in m_local{};            // bind した自分のアドレス（主にポート情報）
    sockaddr_in m_remote{};           // 送信先アドレス（SetRemoteで設定）
    bool m_hasRemote{ false };        // 送信先が設定されているかどうか

private:
    // FormatWSAError:
    // WSAGetLastError() などのエラーコードを「人が読める文字列」に変換する
    // （FormatMessageAを使う実装を .cpp 側に置く想定）
    static std::string FormatWSAError(int err);

    // SetErr:
    // outErr があればメッセージを書き込み、false を返すユーティリティ
    // 失敗returnを書くのが楽になる（return SetErr(...); の形）
    static bool SetErr(std::string* outErr, const std::string& msg);

    // MoveFrom:
    // ムーブ構築/ムーブ代入で使う「所有権移動」の中身
    // rhs の資源を奪い、rhs を安全な空状態にする
    void MoveFrom(UDPCOM&& rhs) noexcept;
};
