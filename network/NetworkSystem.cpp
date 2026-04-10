#include "NetworkSystem.h"
#include <cstring>   // std::memset
#include <sstream>


namespace {
    static std::string PeerToString(const NetworkSystem::PeerEndpoint & p)
    {
    std::ostringstream oss;
    oss << p.ip << ":" << p.port;
    return oss.str();
    }
}
//------------------------------------------------------------------------------
// Start
//  UDP 通信を開始し、受信スレッドを立ち上げる。
//  - Open        : Winsock初期化/ソケット生成など
//  - Bind        : 自分の受信ポートを確保（受信するなら必須）
//  - SetRemote   : 送信先（相手IP/Port）を登録（Sendが簡単になる）
//  - Broadcast   : ブロードキャスト送信を許可（必要なら）
//  - ConnReset無効: Windows特有の recvfrom が WSAECONNRESET になる問題の回避策
//  - NonBlocking : Stop() で止めやすくするための重要設定（推奨）
//------------------------------------------------------------------------------
bool NetworkSystem::Start(uint16_t myPort, const char* yourIp, uint16_t yourPort, ErrorSink onError)
{
    // エラー通知コールバックを保存
    // ※受信スレッド(RecvLoop)からもエラーを通知したいのでメンバに保持する
    m_onError = std::move(onError);

    // UDPCOM を生成（Start/Stop で寿命管理）
    m_udp = std::make_unique<UDPCOM>();

    std::string err;

    // 1) ソケット生成などの初期化
    // 失敗したらネットワーク開始できないので Start 失敗として返す
    if (!m_udp->Open(&err)) {
        if (m_onError) m_onError("Start(Open) failed: " + err);
        return false;
    }

    // 2) 受信するなら bind が必須
    // このポート(myPort)で UDP パケットを受け取る
    // ※bindしないと OS がどのポートで受けるか決まらず受信できない
    if (!m_udp->Bind(myPort, &err)) {
        if (m_onError) m_onError("Start(Bind myPort=" + std::to_string(myPort) + ") failed: " + err);
        return false;
    }

    // 3) 送信先（相手）を登録
    // UDPCOM 内部に相手アドレスを保持させることで、
    // Send() のたびに sockaddr_in を作る必要がなくなる
    if (!m_udp->SetRemote(yourIp, yourPort, &err)) {
        if (m_onError) {
            m_onError("Start(SetRemote ip=" + std::string(yourIp ? yourIp : "(null)")
                 +" port=" + std::to_string(yourPort) + ") failed: " + err);
        } 
        return false;
    }

    // 4) （任意）ブロードキャスト送信を使うなら有効化
    // ※使わないなら不要。失敗しても致命ではない場合が多い
    m_udp->SetBroadcast(true, &err);
    if (!err.empty() && m_onError) m_onError("Start(SetBroadcast) warning: " + err);

    // 5) UDP connreset 無効化（Windowsでよくある落ち対策）
    // - 相手側が存在しない/ポートが閉じている等で ICMP が返ると、
    //   次の recvfrom が WSAECONNRESET で失敗することがある
    // - 無効化できない環境もあるので、ここでは「致命扱いにせず」通知だけする
    if (!m_udp->DisableUdpConnReset(&err)) {
        if (m_onError) m_onError(err);
    }

    // 6) non-blocking 設定（非常に重要）
    // - blocking recv だと受信待ちで止まったままになり、
    //   Stop() してもスレッドが抜けられず join が返ってこない危険がある
    // - non-blocking なら「データが無いとき」WSAEWOULDBLOCK になり、
    //   ループ内で m_running を見ながら安全に抜けられる
    if (!m_udp->SetNonBlocking(true, &err)) {
        if (m_onError) m_onError("Start(SetNonBlocking) failed: " + err);
        return false;
    }

    // 7) 受信スレッド開始
    // m_running を true にしてから thread を起動する（順序が大事）
    m_running = true;
    m_thread = std::thread(&NetworkSystem::RecvLoopWithoutbusy, this);
    if (m_onError) {
        m_onError("Start success: local UDP port=" + std::to_string(myPort) + ", default remote=" + std::string(yourIp ? yourIp : "(null)") + ":" + std::to_string(yourPort));
    }

    return true;
}

//------------------------------------------------------------------------------
// Stop
//  受信スレッドを停止して安全に後始末する。
//  - m_running=false でループ終了を指示
//  - join でスレッド終了を待つ（スレッドが動いたまま破棄すると危険）
//  - UDPソケットを Close して破棄
//  - 受信キューに残ったデータをクリア
//------------------------------------------------------------------------------
void NetworkSystem::Stop()
{
    // 1) 受信ループ終了フラグ
    // RecvLoop がこのフラグを見て while を抜ける
    m_running = false;

    // 2) スレッドが起動しているなら終了待ち
    // non-blocking + Sleep によって比較的すぐ抜ける設計
    if (m_thread.joinable()) {
        m_thread.join();
    }

    // 3) ソケットを閉じて破棄
    if (m_udp) {
        m_udp->Close();
        m_udp.reset();
    }

    // 4) 受信キューを掃除（未処理データの破棄）
    // Stop 後に古いメッセージが残ると事故るので空にする
    {
        std::lock_guard<std::mutex> lock(m_inMutex);
        while (!m_inQueue.empty()) m_inQueue.pop();
    }
}

//------------------------------------------------------------------------------
// RegisterHandler
//  メッセージ種別ごとの処理関数（ハンドラ）を登録する。
//  - 受信した MsgData は PumpIncoming() 経由で dispatcher に渡され、
//    type に応じて登録された handler が呼ばれる。
//------------------------------------------------------------------------------
void NetworkSystem::RegisterHandler(
    MessageType type,
    std::function<void(std::unique_ptr<MsgData>, uint32_t, uint16_t)> handler)
{
    // dispatcher に登録（所有権を move）
    m_dispatcher.registerHandler(type, std::move(handler));
}

//------------------------------------------------------------------------------
// Send
//  相手（Startで SetRemote した先）に MsgData を送る。
//  - msg を生バイト列として送っている点に注意（構造体送信）。
//    同一環境前提なら簡単だが、将来はシリアライズ推奨。
//------------------------------------------------------------------------------
bool NetworkSystem::Send(const MsgData& msg)
{
    // Start前/Stop後なら送れない
    if (!m_udp) {
        if (m_onError) m_onError("Send failed: UDP is not started");
        return false;
    }
    
    std::string err;

    // MsgData をそのまま送信
    // UDPCOM::Send は成功時に送信サイズ、失敗時に SOCKET_ERROR を返す想定
    int sts = m_udp->Send(&msg, sizeof(MsgData), &err);

    if (sts == SOCKET_ERROR) {
        // err があるなら通知（UDPCOM側で FormatMessage 等している想定）
        if (m_onError && !err.empty()) m_onError(err);
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// SendAll
//  peers 全員に MsgData を送る。
//  - UDPCOM::SendTo を使うので、SetRemote 上書き不要（複数宛先に安全）。
//  - 途中失敗しても残りは送る。
//------------------------------------------------------------------------------
bool NetworkSystem::SendAll(const MsgData& msg)
{
    if (!m_udp) {
        if (m_onError) m_onError("SendAll failed: UDP is not started");
        return false;
    }

    if (m_peers.empty()) {
        if (m_onError) m_onError("SendAll: peer list is empty");
        return false;
    }

    bool anySuccess = false;
    bool anyFailure = false;

    for (std::size_t i = 0; i < m_peers.size(); ++i) {
        std::string err;

        // MsgData をそのまま送信（構造体送信）
        int sts = m_udp->SendTo(&msg, sizeof(MsgData), m_peers[i].address, &err);

        if (sts == SOCKET_ERROR) {
            anyFailure = true;
            if (m_onError) {
                if (!err.empty()) {
                    m_onError("SendAll: SendTo failed peers[" + std::to_string(i) + "] " + PeerToString(m_peers[i]) + ": " + err);
                }
                else {
                    m_onError("SendAll: SendTo failed peers[" + std::to_string(i) + "] " + PeerToString(m_peers[i]));
                }
            }
            continue; // 残りへ
        }

        anySuccess = true;
    }

    // 全員失敗なら false
    if (!anySuccess) return false;

    // 必要なら部分失敗をまとめて通知
    // if (anyFailure && m_onError) m_onError("SendAll: partial failure");

    return true;
}

//------------------------------------------------------------------------------
// RecvLoop（受信スレッド）
//  別スレッドで回り続け、受信したメッセージをキューに積む。
//  - 受信処理はネットワークスレッドで行う
//  - ゲームロジック（ハンドラ呼び出し）はメインスレッドで行う
//    → その橋渡しが「m_inQueue」
//------------------------------------------------------------------------------
void NetworkSystem::RecvLoop()
{
    // m_running が true の間、受信を繰り返す
    while (m_running) {

        // 受信バッファ（MsgData 1個分）
        // unique_ptr にしておくと、キューへ move で安全に渡せる
        auto buf = std::make_unique<MsgData>();

        // 念のためゼロクリア（未使用領域やパディングのゴミを防ぐ）
        std::memset(buf.get(), 0, sizeof(MsgData));

        // 送信元アドレス（Recvfrom で埋められる）
        sockaddr_in from{};
        std::string err;

        // UDP 受信（non-blocking）
        // 戻り値の意味（想定）：
        //  - recvsize > 0          : 受信成功（受信したバイト数）
        //  - recvsize == 0         : 通常UDPでは稀（実装によってはあり得る）
        //  - recvsize == SOCKET_ERROR: 失敗（non-blockingなら WSAEWOULDBLOCK が多い）
        int recvsize = m_udp->Recv(buf.get(), sizeof(MsgData), &from, &err);

        // 受信失敗
        if (recvsize == SOCKET_ERROR) {

            // non-blocking の場合、
            // データが無いだけで WSAEWOULDBLOCK になるのが普通
            // UDPCOM 側で "WOULDBLOCK は err 空で返す" 設計なら、ここで何もしない
            if (!err.empty() && m_onError) m_onError(err);

            // ビジーループ抑制（CPU 100% を避ける）
            ::Sleep(1);
            continue;
        }

        // 受信成功（何か来た）
        if (recvsize > 0) {

            // キュー投入用のアイテムを作る
            IncomingItem item;
            item.msg = std::move(buf);                 // 受信データ本体（所有権移動）
            item.ip = from.sin_addr.S_un.S_addr;      // 送信元IPv4（DWORD）
            item.port = from.sin_port;                // 注意：network byte order
            // 必要なら ntohs() してから使う

        // キューは複数スレッドから触るので mutex で保護
            std::lock_guard<std::mutex> lock(m_inMutex);
            m_inQueue.push(std::move(item));
        }
        else {
            // recvsize == 0 の場合（基本は来ないが保険）
            ::Sleep(1);
        }
    }
}

//------------------------------------------------------------------------------
// RecvLoopWithoutbusy
//  select() を使って「受信データが来るまで待つ」ことでビジーループを避ける版。
//  - non-blocking + Sleep(1) の方式よりCPU負荷が小さい
//  - タイムアウトを設けて定期的に m_running を確認し、Stop() に反応できる
//------------------------------------------------------------------------------
void NetworkSystem::RecvLoopWithoutbusy()
{
    // 受信スレッドは m_running が true の間ループする
    while (m_running) {

        //    監視対象ソケットを取り出す（毎回同じなら外に出してもOK）
        const SOCKET s = m_udp ? m_udp->NativeSocket() : INVALID_SOCKET;
        if (s == INVALID_SOCKET) {
            // ソケットが無い/壊れているなら少し待ってリトライ
            ::Sleep(1);
            continue;
        }

        //     select() に渡す fd_set を準備
        //     select() は fd_set を「書き換える」ので毎ループ作り直す必要がある
        fd_set readfds{};
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);

        // タイムアウト設定
        //     - ここで待っている間はスレッドが寝る（CPUほぼ使わない）
        //     - ただし無限待ちにすると Stop() しても起きないので
        //       定期的に起きるためタイムアウトを設定する
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms（Stop の反応速度とCPU負荷のトレードオフ）

        // 待機（読み込み可能になるまで or タイムアウト）
        //     Windowsでは第1引数は無視されるので 0 でOK
        const int ret = select(0, &readfds, nullptr, nullptr, &tv);

        if (ret == SOCKET_ERROR) {
            // select 自体が失敗
            // 本当は WSAGetLastError() で原因をログに出すのが理想
            // 例：int e = WSAGetLastError();
            ::Sleep(1); // エラーが連発する場合の暴走抑制
            continue;
        }

        if (ret == 0) {
            // タイムアウト：データは来なかった
            // → while先頭へ戻って m_running を確認する（Stopに反応するため）
            continue;
        }

        // (ここに来た＝少なくとも1つのソケットが読み込み可能
        //  今回は監視ソケットが1つなので、基本的に「s が読める」状態
        //  念のため FD_ISSET で確認しても良い
        if (!FD_ISSET(s, &readfds)) {
            // 通常ここには来ないが、保険
            continue;
        }

        // 実際に受信（UDPは recvfrom 相当）
        auto buf = std::make_unique<MsgData>();
        std::memset(buf.get(), 0, sizeof(MsgData));

        sockaddr_in from{};
        std::string err;

        const int recvsize = m_udp->Recv(buf.get(), sizeof(MsgData), &from, &err);

        if (recvsize == SOCKET_ERROR) {
            // select の後でも recv が失敗することはあり得る（Closeされた等）
            if (!err.empty() && m_onError) m_onError(err);
            continue;
        }

        if (recvsize <= 0) {
            // 0はUDPだと通常は来ないが、実装差/異常ケースの保険
            continue;
        }

        // 受信成功：メインスレッドへ渡すためキューへ積む
        IncomingItem item;
        item.msg = std::move(buf);
        item.ip = from.sin_addr.S_un.S_addr;
        item.port = ntohs(from.sin_port); // ★ホストオーダで

        // キューは受信スレッドとメインスレッドで共有するので mutex で保護
        {
            std::lock_guard<std::mutex> lock(m_inMutex);
            m_inQueue.push(std::move(item));
        }
    }
}

//------------------------------------------------------------------------------
// PumpIncoming（メイン/ゲームスレッドで呼ぶ）
//  受信スレッドが積んだキューを取り出して、dispatcher に流す。
//  ポイント：
//  - ロックしている時間を短くするため、キューを local に swap してから処理する
//    → ロック中に handler を呼ぶと、重い処理で受信スレッドが詰まるから
//------------------------------------------------------------------------------
void NetworkSystem::PumpIncoming()
{
    // ローカルキューへ移す（この間だけロック）
    std::queue<IncomingItem> local;
    {
        std::lock_guard<std::mutex> lock(m_inMutex);        // ミューテックスを利用してロック
        std::swap(local, m_inQueue);                        // ローカルにコピー
    }

    // ロック不要領域で dispatch（安全＆高速）
    while (!local.empty()) {
        auto& it = local.front();                           // キュー取り出し

        // dispatcher が type を見て登録済み handler を呼ぶ
        // msg は unique_ptr なので move して所有権を渡す
        m_dispatcher.dispatch(
            std::move(it.msg),
            it.ip,                                          // 送信元IP
            it.port);                                       // 送信元ポート

        local.pop();                                        // キューから削除
    }
}

// 例：NetworkSystem クラス内の private 関数として
bool NetworkSystem::ResolvePeerAddress(PeerEndpoint& peer, std::string* outErr)
{
    peer.setted = false; // 失敗時に残らないように最初に落とす

    if (peer.ip.empty()) {
        if (outErr) *outErr = "peer.ip is empty";
        return false;
    }
    if (peer.port == 0) {
        if (outErr) *outErr = "peer.port is 0";
        return false;
    }

    // sockaddr_in を初期化
    std::memset(&peer.address, 0, sizeof(peer.address));
    peer.address.sin_family = AF_INET;
    peer.address.sin_port = htons(peer.port);

    // IP文字列 → in_addr 変換
    // 成功: 1 / 失敗: 0 / エラー: -1
    const int r = InetPton(AF_INET, peer.ip.c_str(), &peer.address.sin_addr);
    if (r != 1) {
        if (outErr) {
            if (r == 0) {
                *outErr = "invalid IPv4 address string: " + peer.ip;
            }
            else {
                *outErr = "InetPton failed (WSA error=" + std::to_string(WSAGetLastError()) + "): " + peer.ip;
            }
        }
        return false;
    }

    peer.setted = true;
    return true;
}

bool NetworkSystem::AddPeer(const std::string& ip, uint16_t port, std::string* outErr)
{
    if (m_peers.size() >= 10) {
        if (outErr) *outErr = "too many peers (max 10): request=" + ip + ":" + std::to_string(port);        return false;
    }

    PeerEndpoint p;
    p.ip = ip;
    p.port = port;

    // ここで変換
    std::string err;
    if (!ResolvePeerAddress(p, &err)) {
        if (outErr) *outErr = "AddPeer(" + ip + ":" + std::to_string(port) + "): " + err;        return false;
    }

    // 変換できたものだけ登録
    m_peers.push_back(std::move(p));
    return true;
}
