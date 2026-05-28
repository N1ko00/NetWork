#pragma once

#include "system/IScene.h"
#include "system/SceneClassFactory.h"
#include "system/CSprite.h"

#include <memory>
#include <array>
#include <string>
#include "ConnectionSettings.h"
#include "matching/MatchingClient.h"

class TitleScene : public IScene {
public:
    TitleScene() = default;

    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void init() override;
    void dispose() override;

    Camera* GetCamera() override { return nullptr; }
    uint64_t getmachineid() override { return 0; }

private:
    std::unique_ptr<CSprite> m_backgroundSprite;
    std::unique_ptr<CSprite> m_startSprite;
    std::unique_ptr<CSprite> m_exitSprite;

    //マッチングUI状態 enumで管理
    enum class MatchingUIState
    {
        Idle,
		SelectingPort,
		Matching,
		Waiting,
        CreatingRoom,
        WaitingForJoin,
        JoiningRoom,
        Matched,
        Failed,
        Timeout,
		Cancelled,
	};

    //Server設定
    std::array<char, 64>m_serverIpInput{};
	int m_serverPortInput = 8080;

    //Host用入力
	int m_hostMyPortInput = 49152;

	//Join用入力
	int m_joinMyPortInput = 49153;

	//Manual用入力
    int m_myPortInput = 50001;
    int m_remotePortInput = 50000;
    std::array<char, 64> m_remoteIpInput{};
	std::array<char, 64> m_roomIdInput{};
    std::string m_hostToken{};
	std::string m_statusMessage{};
	std::string m_errorMessage{};
	MatchingEndpoint m_lastMatchedEndpoint{};
	MatchingClient m_matchingClient;
	MatchingUIState m_matchingUIState = MatchingUIState::Idle;
	uint64_t m_pollAccumMs = 0;
	uint64_t m_waitAccumMs = 0;
	uint64_t m_pollIntervalMs = 1000;
	uint64_t m_waitTimeoutMs = 120000;
	uint64_t m_joinRequestTimeoutMs = 8000;  //join API待機の目安
	uint64_t m_pollMaxWaitMs = 12000; //poll APIの最大待機時間の目安
	bool m_cancelInFlight = false;  //cacel実行中のUI制御

	bool ApplyAndGoToP2PScene();    //Manual入力をConnectionSettingsStoreに反映してP2PSceneへ
	//ApplyAndGoToP2PSceneの上位互換　マッチングで得たendpointをConnectionSettingsStoreに反映してP2PSceneへ
	bool ApplyRemoteAndGoToP2PScene(const std::string& remoteIp, int remotePort, int myPort, const char* sourceLabel);
	void UpdateHostPolling(uint64_t delta);  //Host待機中は定期的にマッチングサーバーにpollする

	std::string BuildServerUrl()const;  //マッチングサーバーURLを組み立てる
	const char* MatchingStateLabel() const;  //マッチングUI状態を文字列に変換する
	bool IsHostWaiting() const;  //現在ホスト待機中かどうか
	void SetState(MatchingUIState state, const std::string& message = {});  //マッチングUI状態を変更する　必要に応じて状態遷移の初期化も行う
	void SetError(const std::string& rawError);  //エラーメッセージをセットする　必要に応じて状態遷移も行う
	std::string ToUiErrorMessage(const std::string& rawError) const;  //APIの生エラーメッセージをUI向けに変換する
	void StartCreateRoom();  //マッチングサーバーに部屋を作成してもらう　成功すればホスト待機状態へ遷移
	void StartJoinRoom();  //マッチングサーバーに部屋に入れてもらう　成功すればP2Pシーンへ遷移
	void CancelHostWaiting();  //ホスト待機をキャンセルしてIdle状態へ
	void DrawMatchingUi();  //マッチングUIの描画
	void DrawConnectionStatusUi();  //接続状態表示UIの描画

	bool TryParseServerErrorCode(const std::string& raw, std::string& outCode)const;  //APIの生エラーからエラーコードを抜き取る
	void SaveUiToConnectionSettings();  //UIの入力をConnectionSettingsStoreに保存する

	//Auto Matching用
	std::string m_autoTicketId{};
	int m_autoSelectedPort = 0;
	bool m_autoMatchingInFlight = false;
	uint64_t m_autoPollAccumMs = 0;
	uint64_t m_autoPollIntervalMs = 1000;
	uint64_t m_autoWaitAccumMs = 0;
	uint64_t m_autoWaitTimeoutMs = 120000;

	//Auto Matching 空きUDPポート探索
	bool SelectFreeUdpPortInRange(int beginPort, int endPort, int& outPort, std::string& outErr);
	void StartAutoMatching();
	void UpdateAutoMatchingPolling(uint64_t delta);
	void CancelAutoMatching();
};

REGISTER_CLASS(TitleScene)