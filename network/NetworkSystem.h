#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <string>
#include <memory>

#include <winsock2.h>
#include "udpcom.h"

// 例：あなたの MsgData / MessageType に合わせて調整
#include "message.h"
#include "CMessageDispacher.h"

class NetworkSystem {
public:
    using ErrorSink = std::function<void(const std::string&)>; // UI/ログ出力先

    NetworkSystem() = default;
    ~NetworkSystem() { Stop(); }

    NetworkSystem(const NetworkSystem&) = delete;
    NetworkSystem& operator=(const NetworkSystem&) = delete;

    bool Start(uint16_t myPort, const char* yourIp, uint16_t yourPort, ErrorSink onError = nullptr);
    void Stop();

    // Scene側：毎フレーム呼んで、受信分をゲームスレッドで処理する
    void PumpIncoming();

    // 送信
    bool Send(const MsgData& msg);
    bool SendAll(const MsgData& msg);

    // ハンドラ登録（Scene初期化時）
    void RegisterHandler(MessageType type,
        std::function<void(std::unique_ptr<MsgData>, uint32_t, uint16_t)> handler);

    // 通信相手の宛先情報
    struct PeerEndpoint {
        bool setted{ false };               // アドレス変換完了
        std::string ip;                     // IPアドレス
        uint16_t port;                      // ポート番号
        sockaddr_in address;                // アドレス
    };

    // 通信相手を登録
    bool AddPeer(const std::string& ip, uint16_t port, std::string* outErr);

private:
    void RecvLoop();
    void RecvLoopWithoutbusy();

    bool ResolvePeerAddress(PeerEndpoint& peer, std::string* outErr);

private:
    std::unique_ptr<UDPCOM> m_udp;
    std::thread m_thread;
    std::atomic_bool m_running{ false };

    // 受信→ゲームスレッドへ渡すキュー
    struct IncomingItem {
        std::unique_ptr<MsgData> msg;
        uint32_t ip{};
        uint16_t port{};
    };
    std::mutex m_inMutex;
    std::queue<IncomingItem> m_inQueue;

    CMessageDispacher m_dispatcher;
    ErrorSink m_onError;

    // 宛先情報（最大10件）
    std::vector<PeerEndpoint>   m_peers;    // 宛先情報
};
