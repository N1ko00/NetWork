// ------------------------------------------------------------
// このクラスがやっていること（ざっくり）
//  1) Open()  : Winsock を初期化して UDPソケットを作る
//  2) Bind()  : 自分が受信するポート番号を決める（受信するなら必須）
//  3) SetRemote(): 送信先のIP/ポートを登録（Send() を簡単にするため）
//  4) Send()  : UDPで送る（到達保証なし）
//  5) Recv()  : UDPで受ける（ノンブロッキングならデータ無しで 0 を返す）
//  6) Close() : 後片付け（ソケット閉じる + WSACleanup）
//
// 重要：UDPは「到達保証なし」「順序保証なし」
// ------------------------------------------------------------

#include "udpcom.h"
#include <sstream>
#include <windows.h>   // FormatMessageA, LocalFree など（エラー説明文を作るため）
#include <limits>

// 古いSDKなどでは SIO_UDP_CONNRESET が定義されていない場合があるので自前定義する。
// ※SIO_UDP_CONNRESET は Windows の UDP 特有の挙動（ICMPで recvfrom が落ちる）を制御するための番号。
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace
{
    inline bool TrySizeTToInt(std::size_t v, int& out)
    {
        if (v > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return false;
        out = static_cast<int>(v);
        return true;
    }
}

//============================================================
// Open()
// 目的：UDP通信を開始できる状態にする
//   - WSAStartup() で Winsock（Windowsの通信機能）を初期化
//   - socket() で UDPソケットを作る
//============================================================
bool UDPCOM::Open(std::string* outErr)
{
    // すでにソケットが作られている＝Open済みなら何もしない
    if (m_sock != INVALID_SOCKET) return true;

    // Winsock を使えるように初期化する（2.2を要求）
    // 成功すると m_wsa にバージョン等の情報が入る
    int sts = WSAStartup(MAKEWORD(2, 2), &m_wsa);
    if (sts != 0) {
        // sts はエラーコード（WSAの番号）
        return SetErr(outErr, "WSAStartup failed: " + FormatWSAError(sts));
    }

    // 初期化はできたが、欲しいバージョン 2.2 が使えない場合は終了
    if (m_wsa.wVersion != MAKEWORD(2, 2)) {
        SetErr(outErr, "Winsock version mismatch (need 2.2).");
        WSACleanup();  // 初期化した分を戻す
        m_wsa = {};
        return false;
    }

    // UDPソケットを作る
    // AF_INET    : IPv4
    // SOCK_DGRAM : UDP（データグラム）
    // IPPROTO_UDP: UDPプロトコル
    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock == INVALID_SOCKET) {
        // socket() が失敗した理由を取得
        int e = WSAGetLastError();
        SetErr(outErr, "socket failed: " + FormatWSAError(e));

        // Open途中で失敗したので後片付け
        WSACleanup();
        m_wsa = {};
        return false;
    }

    return true;
}

//============================================================
// Bind(usePort)
// 目的：自分が受信するポートを OS に予約してもらう
//
// 例：Bind(12345) なら、「12345番ポートで受信」を開始できる。
// usePort = 0 なら OS が空いているポートを自動割り当てする。
//============================================================
bool UDPCOM::Bind(uint16_t usePort, std::string* outErr)
{
    // まだ Open() してないとソケットが無いので bind できない
    if (m_sock == INVALID_SOCKET) {
        return SetErr(outErr, "Bind called before Open().");
    }

    // sockaddr_in：IPv4用のアドレス構造体
    // 受信設定（自分の待ち受け先）を作る
    m_local = {};
    m_local.sin_family = AF_INET;

    // INADDR_ANY は「このPCのどのIPアドレス宛てでも受ける」という意味
    // 0.0.0.0 と同じイメージ
    m_local.sin_addr.s_addr = htonl(INADDR_ANY);

    // htons：ホストのバイト順 -> ネットワークのバイト順に変換
    // ポート番号はネットワークバイトオーダーで渡す必要がある
    m_local.sin_port = htons(usePort);

    // 実際に OS に「このソケットはこのポートで待ち受ける」と登録する
    int sts = bind(m_sock, reinterpret_cast<sockaddr*>(&m_local), sizeof(m_local));
    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();
        return SetErr(outErr, "bind failed: " + FormatWSAError(e));
    }

    return true;
}

//============================================================
// SetRemote(ip, port)
// 目的：送信先（相手）のIPアドレスとポートを保存する
//
// UDPは sendto() で毎回宛先を渡せるが、毎回指定は面倒。
// そこで SetRemote() で宛先を保持しておき、Send() で簡単に送れるようにする。
//============================================================
bool UDPCOM::SetRemote(const char* ip, uint16_t port, std::string* outErr)
{
    if (!ip) return SetErr(outErr, "SetRemote: ip is null.");

    m_remote = {};
    m_remote.sin_family = AF_INET;
    m_remote.sin_port = htons(port);

    // inet_pton： "192.168.0.10" のような文字列IPをバイナリ形式に変換する
    // 戻り値:
    //  1  : 成功
    //  0  : 文字列がIPとして不正
    // -1  : 処理自体の失敗（AFが違う等）
    int ok = inet_pton(AF_INET, ip, &m_remote.sin_addr);
    if (ok != 1) {
        return SetErr(outErr, std::string("inet_pton failed for ip: ") + ip);
    }

    // これで Send() が使える
    m_hasRemote = true;
    return true;
}

//============================================================
// SetBroadcast(enable)
// 目的：ブロードキャスト送信を許可/禁止する
//
// ブロードキャスト（例：255.255.255.255 など）に送るには
// SO_BROADCAST を有効にしておく必要がある。
//============================================================
bool UDPCOM::SetBroadcast(bool enable, std::string* outErr)
{
    if (m_sock == INVALID_SOCKET) return SetErr(outErr, "SetBroadcast called before Open().");

    // setsockopt に渡すのは Windows では BOOL
    BOOL v = enable ? TRUE : FALSE;

    // SO_BROADCAST を ON にするとブロードキャスト送信が可能になる
    int sts = setsockopt(m_sock, SOL_SOCKET, SO_BROADCAST,
        reinterpret_cast<const char*>(&v), sizeof(v));
    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();
        return SetErr(outErr, "setsockopt(SO_BROADCAST) failed: " + FormatWSAError(e));
    }
    return true;
}

//============================================================
// DisableUdpConnReset()
// 目的：WindowsのUDP特有問題への対策
//
// Windowsでは、UDPで送信した先が存在しない（ポートが閉じてる等）と
// ICMP Port Unreachable を受けて recvfrom が WSAECONNRESET で失敗し続けることがある。
// それを抑止する定番対策が SIO_UDP_CONNRESET の設定。
//============================================================
bool UDPCOM::DisableUdpConnReset(std::string* outErr)
{
    if (m_sock == INVALID_SOCKET) return SetErr(outErr, "DisableUdpConnReset called before Open().");

    // FALSE を渡すことで「connresetを起こさない挙動」に変更する（定番）
    BOOL bNewBehavior = FALSE;
    DWORD bytes = 0;

    // WSAIoctl：ソケットの特殊設定を変更する関数
    int sts = WSAIoctl(
        m_sock,
        SIO_UDP_CONNRESET,
        &bNewBehavior, sizeof(bNewBehavior),
        nullptr, 0,
        &bytes,
        nullptr, nullptr);

    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();
        return SetErr(outErr, "WSAIoctl(SIO_UDP_CONNRESET) failed: " + FormatWSAError(e));
    }
    return true;
}

//============================================================
// SetNonBlocking(enable)
// 目的：受信を「待つ」か「待たない」かを切り替える
//
// enable=true（ノンブロッキング）:
//   recvfrom はデータが無いとすぐ戻る（エラーコード WSAEWOULDBLOCK）
// enable=false（ブロッキング）:
//   recvfrom はデータが来るまで止まる
//============================================================
bool UDPCOM::SetNonBlocking(bool enable, std::string* outErr)
{
    if (m_sock == INVALID_SOCKET) return SetErr(outErr, "SetNonBlocking called before Open().");

    // FIONBIO に 1 を渡すとノンブロッキング、0 でブロッキング
    u_long mode = enable ? 1UL : 0UL;

    int sts = ioctlsocket(m_sock, FIONBIO, &mode);
    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();
        return SetErr(outErr, "ioctlsocket(FIONBIO) failed: " + FormatWSAError(e));
    }
    return true;
}

//============================================================
// Send(data, size)
// 目的：UDP送信（SetRemote済みの相手へ）
//
// 戻り値：
//  成功 => 送ったバイト数
//  失敗 => SOCKET_ERROR
//
// 注意：UDPは送れたからといって相手に届く保証は無い（再送制御なし）。
//============================================================
int UDPCOM::Send(const void* data, size_t size, std::string* outErr)
{
    // SetRemote() をしていないと宛先が無い
    if (!m_hasRemote) {
        SetErr(outErr, "Send: remote not set. Call SetRemote().");
        return SOCKET_ERROR;
    }

    // データが無い場合は何もしない（0扱い）
    if (!data || size <= 0) return 0;

    // sendto は int を要求するので、size を安全に int に変換する
    int senddataLen = 0;
    if (!TrySizeTToInt(size, senddataLen)) {
        SetErr(outErr, "sendto: senddataLen overflow (size too large).");
        return SOCKET_ERROR;
    }

    // sendto：UDP送信（宛先を指定して送る）
    int sts = sendto(
        m_sock,
        reinterpret_cast<const char*>(data),
        senddataLen,
        0,
        reinterpret_cast<const sockaddr*>(&m_remote),
        sizeof(m_remote));

    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();
        SetErr(outErr, "sendto failed: " + FormatWSAError(e));
    }
    return sts;
}

//============================================================
// Recv(buf, size, outFrom)
// 目的：UDP受信
//
// 戻り値のルール（ここが重要）:
//  - 受信成功 => 受信したバイト数（1以上）
//  - ノンブロッキングで「まだデータが無い」 => 0 を返す（正常扱い） ★要望対応
//  - それ以外の失敗 => SOCKET_ERROR（outErr に理由）
//
// outFrom:
//  送信元を知りたいなら受け取る（不要なら nullptr でOK）
//============================================================
int UDPCOM::Recv(void* buf, size_t size, sockaddr_in* outFrom, std::string* outErr)
{
    if (!buf || size <= 0) return 0;

    // Open前ならソケットが無いのでエラー
    if (m_sock == INVALID_SOCKET) {
        SetErr(outErr, "Recv called before Open().");
        return SOCKET_ERROR;
    }

    // recvfrom は int を要求するので、size を安全に int に変換する
    int recvbufLen = 0;
    if (!TrySizeTToInt(size, recvbufLen)) {
        SetErr(outErr, "recvfrom: ercvbufLen overflow (size too large).");
        return SOCKET_ERROR;
    }

    // recvfrom：UDP受信。成功すると buf にデータが入り、from に送信元が入る
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int sts = recvfrom(
        m_sock,
        reinterpret_cast<char*>(buf),
        recvbufLen,
        0,
        reinterpret_cast<sockaddr*>(&from),
        &fromLen);

    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();

        // ノンブロッキングで「受信データが無いだけ」の場合は 0 を返す。
        // これにより呼び出し側は「エラーじゃない」「今回は受信無し」と扱える。
        if (e == WSAEWOULDBLOCK) {
            if (outFrom) *outFrom = {}; // 誤用防止（送信元情報をクリア）
            return 0;
        }

        // それ以外は本物のエラー
        SetErr(outErr, "recvfrom failed: " + FormatWSAError(e));
        return SOCKET_ERROR;
    }

    // 成功：送信元も必要なら返す
    if (outFrom) *outFrom = from;
    return sts;
}

//============================================================
// BoundPort()
// 目的：自分が使っている（bindされている）ポート番号を取得
//
// bind(0) のように「OSに任せてポートを決めた」場合に便利。
//============================================================
uint16_t UDPCOM::BoundPort() const
{
    if (m_sock == INVALID_SOCKET) return 0;

    sockaddr_in addr{};
    int len = sizeof(addr);

    // getsockname：このソケットに割り当てられているローカルIP/ポートを取得
    if (getsockname(m_sock, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR) {
        return 0;
    }

    // ntohs：ネットワークバイト順 -> ホストのバイト順に戻す
    return ntohs(addr.sin_port);
}

//============================================================
// Close()
// 目的：後片付け
//  - ソケットを closesocket で閉じる
//  - Winsock を WSACleanup で終了する
//
// 何回呼ばれても壊れないように「状態チェック」してから処理している。
//============================================================
void UDPCOM::Close()
{
    // ソケットが有効なら閉じる
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    // WSAStartup 済みなら WSACleanup する
    // m_wsa.wVersion が 0 でないことを目安にしている
    if (m_wsa.wVersion != 0) {
        WSACleanup();
        m_wsa = {};
    }

    // 状態を初期化して「再Openしやすい」ようにする
    m_hasRemote = false;
    m_local = {};
    m_remote = {};
}

//============================================================
// FormatWSAError(err)
// 目的：WSAエラー番号を「番号: 説明文」の形にする
//
// 例：10035: A non-blocking socket operation could not be completed immediately.
//
// 実装のポイント：FormatMessageA を使う
//  - OSが持つエラーメッセージを取得できる
//  - FORMAT_MESSAGE_ALLOCATE_BUFFER を使うと OS側がメモリ確保してくれるので
//    最後に LocalFree が必要
//============================================================
std::string UDPCOM::FormatWSAError(int err)
{
    LPSTR msgBuf = nullptr;

    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER   // OSにバッファを確保してもらう
        | FORMAT_MESSAGE_FROM_SYSTEM       // OSのメッセージテーブルから取得
        | FORMAT_MESSAGE_IGNORE_INSERTS;   // %1 などの挿入は無視

    // 失敗したら len==0 になる
    DWORD len = FormatMessageA(
        flags,
        nullptr,                   // SYSTEM なので nullptr
        static_cast<DWORD>(err),   // エラー番号
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msgBuf),
        0,
        nullptr);

    // まず「番号」を入れる
    std::ostringstream oss;
    oss << err;

    if (len == 0 || msgBuf == nullptr) {
        // OSが説明文を持っていないコードもあるので、その場合は番号だけ返す
        oss << " (unknown error)";
        return oss.str();
    }

    // msgBuf は OSが確保した文字列（末尾に改行 \r\n が付くことが多い）
    std::string msg(msgBuf, len);

    // OSが確保したメモリは LocalFree で解放する
    LocalFree(msgBuf);

    // 末尾の改行/空白を削って見やすくする
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ' || msg.back() == '\t')) {
        msg.pop_back();
    }

    // 例：「10035: A non-blocking socket operation could not be completed immediately.」
    oss << ": " << msg;
    return oss.str();
}

//============================================================
// SetErr(outErr, msg)
// 目的：エラー文字列を outErr に入れて false を返す「便利関数」
//
// 例：return SetErr(outErr, "bind failed ...");
// と書けるので、毎回 if(outErr) ... しなくてよくなる。
//============================================================
bool UDPCOM::SetErr(std::string* outErr, const std::string& msg)
{
    if (outErr) *outErr = msg;
    return false;
}

//============================================================
// MoveFrom(rhs)
// 目的：ムーブ（所有権移動）の中身
//
// ソケットは「コピーできない資源」なので、ムーブでは
//  - rhs のソケットを自分に渡す（所有権移動）
//  - rhs は空状態にする（デストラクタで二重 close しないため）
//============================================================
void UDPCOM::MoveFrom(UDPCOM&& rhs) noexcept
{
    // rhs の中身を自分へ移す
    m_wsa = rhs.m_wsa;
    m_sock = rhs.m_sock;
    m_local = rhs.m_local;
    m_remote = rhs.m_remote;
    m_hasRemote = rhs.m_hasRemote;

    // rhs は安全な「空」に戻す（ここが重要）
    rhs.m_wsa = {};
    rhs.m_sock = INVALID_SOCKET;
    rhs.m_local = {};
    rhs.m_remote = {};
    rhs.m_hasRemote = false;
}

//============================================================
// SendTo(data, size)
// 目的：指定された宛先にUDP送信
//
// 戻り値：
//  成功 => 送ったバイト数
//  失敗 => SOCKET_ERROR
//
// 注意：UDPは送れたからといって相手に届く保証は無い（再送制御なし）。
//============================================================
int UDPCOM::SendTo(const void* data, size_t size, const sockaddr_in& to, std::string* outErr)
{
    if (m_sock == INVALID_SOCKET) return SetErr(outErr, "UDPCOM::SendTo: invalid socket.");

    // データが無い場合は何もしない（0扱い）
    if (!data || size <= 0) return 0;

    // sendto は int を要求するので、size を安全に int に変換する
    int senddataLen = 0;
    if (!TrySizeTToInt(size, senddataLen)) {
        SetErr(outErr, "sendto: senddataLen overflow (size too large).");
        return SOCKET_ERROR;
    }

    // sendto：UDP送信（宛先を指定して送る）
    int sts = ::sendto(
        m_sock,
        reinterpret_cast<const char*>(data),
        senddataLen,
        0,
        reinterpret_cast<const sockaddr*>(&to),
        sizeof(to));

    if (sts == SOCKET_ERROR) {
        int e = WSAGetLastError();
        SetErr(outErr, "sendto failed: " + FormatWSAError(e));
        return SOCKET_ERROR;
    }

    return sts;
}
