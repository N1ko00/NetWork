#include "TitleScene.h"

#include "Application.h"
#include "system/CDirectInput.h"
#include "system/scenemanager.h"
#include "system/imgui/imgui.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace {
    bool IsValidPort(int p) { return p >= 1 && p <= 65535; }
    
    bool isemptyText(const std::array<char, 64>& text) {
        return text[0] == '\0';
    }
}

bool TitleScene::ApplyAndGoToP2PScene()
{
	SaveUiToConnectionSettings();  //UIの入力をConnectionSettingsStoreに保存する

    if (!IsValidPort(m_myPortInput) || !IsValidPort(m_remotePortInput)) return false;
    if (m_remoteIpInput[0] == '\0') return false;
    
    auto & s = ConnectionSettingsStore::Mutable();
    s.enabled = true;
    s.myPort = m_myPortInput;
    s.remotePort = m_remotePortInput;
    s.remoteIp = m_remoteIpInput.data();
	s.matchingServerIp = m_serverIpInput.data();
    s.matchingServerPort = m_serverPortInput;
	s.matchingServerUrl = BuildServerUrl();

    std::cout<<"[TitleScene][Manual]Start P2P: myport="<<s.myPort
        <<" remote="<<s.remoteIp<<":"<<s.remotePort<<std::endl;
    SceneManager::SetCurrentScene("P2PScene");
    return true;
}

bool TitleScene::ApplyRemoteAndGoToP2PScene(const std::string& remoteIp, int remotePort, int myPort, const char* sourceLabel) {
	SaveUiToConnectionSettings();  //UIの入力をConnectionSettingsStoreに保存する
    
    if (!IsValidPort(myPort) || !IsValidPort(remotePort) || remoteIp.empty()) return false;
	//ConectionSettingsStoreに反映
	//マッチングで取得したendpointをp2pのruntime settingに適用する
	auto& s = ConnectionSettingsStore::Mutable();
	s.enabled = true;
	s.myPort = myPort;
	s.remotePort = remotePort;
	s.remoteIp = remoteIp;
	s.matchingServerIp = m_serverIpInput.data();
	s.matchingServerPort = m_serverPortInput;
	s.matchingServerUrl = BuildServerUrl();
	s.roomId = m_roomIdInput.data();
	s.hostToken = m_hostToken;

    m_lastMatchedEndpoint.ip = remoteIp;
    m_lastMatchedEndpoint.port = remotePort;

	//デバッグ出力
    std::cout<<"[TitleScene][" << (sourceLabel ? sourceLabel : "Matching")
         << "] Start P2P: myPort=" << myPort
         << " remote=" << remoteIp << ":" << remotePort
         << " roomId=" << s.roomId << "\n";
	SceneManager::SetCurrentScene("P2PScene");
	return true;
}

void TitleScene::UpdateHostPolling(uint64_t delta){
    if (!IsHostWaiting()) return;
    if (m_cancelInFlight) return;

    const uint64_t deltaMs = delta / 1000; // usec -> msec
    m_pollAccumMs += deltaMs;
    m_waitAccumMs += deltaMs;

    // Poll全体の最大待機時間
    if (m_waitAccumMs >= m_pollMaxWaitMs) {
        m_hostToken.clear();
        m_pollAccumMs = 0;
        SetState(MatchingUIState::Timeout, "poll max wait timeout");
        m_errorMessage = "timeout";
        std::cerr << "[TitleScene][Host] poll timeout roomId=" << m_roomIdInput.data() << "\n";
        return;
    }

    if (m_pollAccumMs < m_pollIntervalMs) return;
    m_pollAccumMs = 0;

    const std::string serverUrl = BuildServerUrl();
    const std::string roomId = m_roomIdInput.data();

    std::cout << "[TitleScene][Poll][Req] url=" << serverUrl
        << " roomId=" << roomId
        << " hostTokenEmpty=" << (m_hostToken.empty() ? "true" : "false") << "\n";

    auto poll = m_matchingClient.PollRoom(serverUrl, roomId, m_hostToken);

    if (!poll.ok) {
        SetError(poll.error);
        std::cerr << "[TitleScene][Poll][Fail] " << poll.error << "\n";
        return;
    }

    if (!poll.matched) {
        SetState(MatchingUIState::WaitingForJoin, "waiting for join...");
        return;
    }

    SetState(MatchingUIState::Matched, "matched! starting P2P...");
    std::cout << "[TitleScene][Poll][Matched] joinEndpoint="
        << poll.joinEndpoint.ip << ":" << poll.joinEndpoint.port << "\n";

    ApplyRemoteAndGoToP2PScene(
        poll.joinEndpoint.ip,
        poll.joinEndpoint.port,
        m_hostMyPortInput,
        "HostPoll");
}

std::string TitleScene::BuildServerUrl()const {
    std::ostringstream oss;
    oss << "http://" << m_serverIpInput.data() << ":" << m_serverPortInput;
	return oss.str();
}

const char* TitleScene::MatchingStateLabel()const {
    switch(m_matchingUIState) {
    case MatchingUIState::Idle: return "Idle";
    case MatchingUIState::CreatingRoom: return "Creating Room...";
    case MatchingUIState::WaitingForJoin: return "Waiting for Join...";
    case MatchingUIState::JoiningRoom: return "Joining Room...";
    case MatchingUIState::Matched: return "Matched!";
    case MatchingUIState::Failed: return "Failed";
    case MatchingUIState::Timeout: return "Timeout";
	case MatchingUIState::Cancelled: return "Cancelled";
    default: return "Unknown";
	}
}

bool TitleScene::IsHostWaiting()const {
    return m_matchingUIState == MatchingUIState::WaitingForJoin;
}

void TitleScene::SetState(MatchingUIState state, const std::string& message) {
    m_matchingUIState = state;
    m_statusMessage = message;
    if (state != MatchingUIState::Failed && state != MatchingUIState::Timeout) {
        m_errorMessage.clear();
    }
}

std::string TitleScene::ToUiErrorMessage(const std::string& rawError)const {
    std::string code;
    TryParseServerErrorCode(rawError, code);

    // サーバーエラーコード優先でUI文言を決める
    if (code == "room_not_found") return "room not found";
    if (code == "room_full" || code == "room_already_matched") return "room full";
    if (code == "room_expired") return "room expired";
    if (code == "room_cancelled" || code == "room_closed") return "room cancelled";
    if (code == "invalid_port") return "invalid port";
    if (code == "invalid_response" || code == "bad_request") return "invalid response";
    if (code == "timeout") return "timeout";

    // 通信失敗系
    if (rawError.find("connect failed") != std::string::npos ||
        rawError.find("getaddrinfo failed") != std::string::npos ||
        rawError.find("send failed") != std::string::npos ||
        rawError.find("recv failed") != std::string::npos) {
        return "server unreachable";
    }

    if (rawError.find("invalid") != std::string::npos ||
        rawError.find("parse") != std::string::npos) {
        return "invalid response";
    }

    if (rawError.find("timeout") != std::string::npos) {
        return "timeout";
    }

    return rawError.empty() ? "unknown error" : rawError;
}

void TitleScene::SetError(const std::string& rawError){
    m_errorMessage = ToUiErrorMessage(rawError);
    SetState(MatchingUIState::Failed, "operation failed");
    std::cerr << "[TitleScene][MatchingError] " << m_errorMessage
         << " raw=" << rawError << "\n";
}

void TitleScene::StartCreateRoom() {
    //Host処理
    //create呼んでRoomIdとHostToken取得してpoll待機へ
    if(!IsValidPort(m_serverPortInput)||!IsValidPort(m_hostMyPortInput)||m_serverIpInput[0]=='\0') {
        SetError("invalid port");
        return;
	}

	SetState(MatchingUIState::CreatingRoom, "creating room...");
    auto r=m_matchingClient.CreateRoom(BuildServerUrl(), m_hostMyPortInput);
    if (!r.ok) {
        SetError(r.error);
        std::cerr << "[TitleScene][Create][Fail] " << r.error << "\n";
        return;
	}
    if (r.roomId.empty() || r.hostToken.empty()) {
        SetError("parse error");
        return;
    }

	std::fill(m_roomIdInput.begin(), m_roomIdInput.end(), '\0');
	strncpy_s(m_roomIdInput.data(), m_roomIdInput.size(), r.roomId.c_str(), _TRUNCATE);
	m_hostToken = r.hostToken;
    m_pollAccumMs= 0;
	m_waitAccumMs = 0;
	SetState(MatchingUIState::WaitingForJoin, "room created. waiting for join...");
    
	std::cout << "[TitleScene][Host] roomId=" << r.roomId 
        << " hostUdpPort=" << m_hostMyPortInput << "\n";
}

void TitleScene::StartJoinRoom() {
	//join処理
    if(!IsValidPort(m_serverPortInput)||!IsValidPort(m_joinMyPortInput)||m_serverIpInput[0]=='\0') {
        SetError("invalid port");
        return;
	}
    if(isemptyText(m_roomIdInput)) {
        SetError("room not found");
        return;
	}

	SetState(MatchingUIState::JoiningRoom, "joining room...");
    std::cout << "[TitleScene][Join][Req] server=" << BuildServerUrl()
        << " roomId=" << m_roomIdInput.data()
        << " myUdpPort=" << m_joinMyPortInput << "\n";

    auto r=m_matchingClient.JoinRoom(BuildServerUrl(), m_roomIdInput.data(), m_joinMyPortInput);
    if (!r.ok) {
        //join失敗はUIでFailedかTimeoutを分ける
        if (ToUiErrorMessage(r.error) == "timeout") {
            SetState(MatchingUIState::Timeout, "join timeout");
			m_errorMessage = "timeout";
        }
        else {
            SetError(r.error);
        }
		std::cerr << "[TitleScene][Join][Fail] " << r.error << "\n";
        return;
    }

	SetState(MatchingUIState::Matched, "joined. starting P2P...");
    if (!ApplyRemoteAndGoToP2PScene(r.hostEndpoint.ip, r.hostEndpoint.port, m_joinMyPortInput, "Join")) {
        SetError("invalid host endpoint");
    }
}

void TitleScene::CancelHostWaiting() {
    if (!IsHostWaiting()) return;

    m_cancelInFlight = true;

    // Cancel処理:
    // ルームを閉じてJoin不能にする（サーバー側 /cancel API）
    std::string err;
    const bool ok = m_matchingClient.CancelRoom(
        BuildServerUrl(),
        m_roomIdInput.data(),
        m_hostToken,
        err);

    if (!ok) {
        SetError(err);
        m_cancelInFlight = false;
        return;
    }

    m_hostToken.clear();
    m_pollAccumMs = 0;
    m_waitAccumMs = 0;
    m_cancelInFlight = false;

    SetState(MatchingUIState::Cancelled, "host waiting cancelled");
    std::cout << "[TitleScene][Host] cancelled roomId=" << m_roomIdInput.data() << "\n";
}

bool TitleScene::TryParseServerErrorCode(const std::string& raw, std::string& outCode) const
{
    // {"error":{"code":"xxx","message":"..."}} から code を簡易抽出する
    const std::string pat = "\"code\":\"";
    const auto p = raw.find(pat);
    if (p == std::string::npos) return false;
    const auto s = p + pat.size();
    const auto e = raw.find('"', s);
    if (e == std::string::npos) return false;
    outCode = raw.substr(s, e - s);
    return true;
}

void TitleScene::SaveUiToConnectionSettings() {
	auto& s = ConnectionSettingsStore::Mutable();
	s.matchingServerIp = m_serverIpInput.data();
    s.matchingServerPort = m_serverPortInput;
    s.matchingServerUrl = BuildServerUrl();
    s.roomId = m_roomIdInput.data();
    s.hostToken = m_hostToken;
}

void TitleScene::update(uint64_t delta)
{
    const float screenWidth = static_cast<float>(Application::GetWidth());
    const float screenHeight = static_cast<float>(Application::GetHeight());
    const float buttonWidth = screenWidth * 0.24f;
    const float buttonHeight = buttonWidth * 0.35f;
    const float buttonY = screenHeight * 0.78f;
    const float exitButtonX = screenWidth * 0.32f;
    const float startButtonX = screenWidth * 0.68f;

    
	//Host待機中は定期的にマッチングサーバーにpollする
	UpdateHostPolling(delta);

	//enterは既存の設定でP2Pシーンへ
    if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
        ApplyAndGoToP2PScene();
        return;
	}

    if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_ESCAPE)) {
        PostQuitMessage(0);
    }

    if (CDirectInput::GetInstance().GetMouseLButtonTrigger()) {
        const float mouseX = static_cast<float>(CDirectInput::GetInstance().GetMousePosX());
        const float mouseY = static_cast<float>(CDirectInput::GetInstance().GetMousePosY());

        auto isInsideButton = [mouseX, mouseY, buttonWidth, buttonHeight](float centerX, float centerY) {
            const float left = centerX - (buttonWidth * 0.5f);
            const float right = centerX + (buttonWidth * 0.5f);
            const float top = centerY - (buttonHeight * 0.5f);
            const float bottom = centerY + (buttonHeight * 0.5f);

            return (mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom);
        };

        if (isInsideButton(startButtonX, buttonY)) {
            ApplyAndGoToP2PScene();
            return;
        }

        if (isInsideButton(exitButtonX, buttonY)) {
            PostQuitMessage(0);
        }
    }
}

void TitleScene::draw(uint64_t delta)
{
    (void)delta;

    const float screenWidth = static_cast<float>(Application::GetWidth());
    const float screenHeight = static_cast<float>(Application::GetHeight());

    if (m_backgroundSprite) {
        m_backgroundSprite->Draw(
            Vector3(1.0f, 1.0f, 1.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(screenWidth * 0.5f, screenHeight * 0.5f, 0.0f));
    }

    const float buttonY = screenHeight * 0.78f;
    const float exitButtonX = screenWidth * 0.32f;
    const float startButtonX = screenWidth * 0.68f;

    if (m_exitSprite) {
        m_exitSprite->Draw(
            Vector3(1.0f, 1.0f, 1.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(exitButtonX,buttonY, 0.0f));
    }

    if (m_startSprite) {
        m_startSprite->Draw(
            Vector3(1.0f, 1.0f, 1.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(startButtonX,buttonY, 0.0f));
    }

    DrawMatchingUi();
}

void TitleScene::DrawConnectionStatusUi(){
    // 状態表示:
    // 現在の状態・エラー・最後に取得したendpointをTitleScene上で確認できるようにする。
    ImGui::Separator();
    ImGui::Text("State: %s", MatchingStateLabel());
    if (!m_statusMessage.empty()) {
        ImGui::Text("Info: %s", m_statusMessage.c_str());
        
    }
    if (!m_errorMessage.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Error: %s", m_errorMessage.c_str());
    }
    if (!m_lastMatchedEndpoint.ip.empty()) {
        ImGui::Text("Last endpoint: %s:%d", m_lastMatchedEndpoint.ip.c_str(), m_lastMatchedEndpoint.port);
    }
    if (IsHostWaiting()) {
        ImGui::Text("RoomID: %s", m_roomIdInput.data());
        ImGui::Text("Waiting: %llu / %llu ms",
        static_cast<unsigned long long>(m_waitAccumMs),
        static_cast<unsigned long long>(m_waitTimeoutMs));
    }
}

void TitleScene::DrawMatchingUi(){
    ImGui::Begin("P2P Connection Setup");
    
    // Server設定:
    // URLを直接打たせず、IPとPortを分けて入力ミスを減らす。
    if (ImGui::CollapsingHeader("Server Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InputText("Server IP", m_serverIpInput.data(), static_cast<int>(m_serverIpInput.size()));
    ImGui::InputInt("Server Port", &m_serverPortInput);
    ImGui::Text("Server URL: %s", BuildServerUrl().c_str());    
    }
    
    // Host:
    // 自分のUDP受信portを入力し、Room作成後はJoin待ち状態へ移行する。
    if (ImGui::CollapsingHeader("Host", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Host My UDP Port", &m_hostMyPortInput);
        const bool canCreate = !IsHostWaiting();
        if (!canCreate) ImGui::BeginDisabled();
        if (ImGui::Button("Create Room")) {
            StartCreateRoom();
        }
        if (!canCreate) ImGui::EndDisabled();
        ImGui::Text("Created RoomID: %s", m_roomIdInput.data());
        ImGui::Text("Host State: %s", MatchingStateLabel());
        if (IsHostWaiting()) {
            ImGui::SameLine();
			if (m_cancelInFlight)ImGui::BeginDisabled();
            if (ImGui::Button("Cancel")) {
                CancelHostWaiting();
            }
			if (m_cancelInFlight) ImGui::EndDisabled();
        }
    }
	// HostTokenはHostがJoinを待つために必要なトークン。
    ImGui::Text("HostToken: %s", m_hostToken.empty() ? "(empty)" : m_hostToken.c_str());
    ImGui::Text("PollInterval(ms): %llu", static_cast<unsigned long long>(m_pollIntervalMs));
    ImGui::Text("PollMaxWait(ms): %llu", static_cast<unsigned long long>(m_pollMaxWaitMs));

    // Join:
    // RoomIDと自分のUDP受信portを入力し、成功したらHost endpointでP2Pへ遷移する。
    if (ImGui::CollapsingHeader("Join", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Join RoomID", m_roomIdInput.data(), static_cast<int>(m_roomIdInput.size()));
        ImGui::InputInt("Join My UDP Port", &m_joinMyPortInput);
        if (ImGui::Button("Join Room")) {
            StartJoinRoom();
            
        }
    }
    // Manual Connect:
    // MatchingServerが使えない時のfallbackとして既存の手動IP入力を残す。
    if (ImGui::CollapsingHeader("Manual Connect Fallback", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Manual My UDP Port", &m_myPortInput);
        ImGui::InputText("Manual Remote IP", m_remoteIpInput.data(), static_cast<int>(m_remoteIpInput.size()));
        ImGui::InputInt("Manual Remote Port", &m_remotePortInput);
        ImGui::Text("Manual endpoint: my=%d remote=%s:%d", m_myPortInput, m_remoteIpInput.data(), m_remotePortInput);
        if (ImGui::Button("Manual Start")) {
            if (!ApplyAndGoToP2PScene()) {
                SetError("invalid port");
            }
        }
    }
    DrawConnectionStatusUi();
    ImGui::End();
}

void TitleScene::init()
{
    m_backgroundSprite = std::make_unique<CSprite>(
        static_cast<int>(Application::GetWidth()),
        static_cast<int>(Application::GetHeight()),
        "assets/texture/haikei.jpg");

    const int buttonWidth = static_cast<int>(Application::GetWidth() * 0.24f);
    const int buttonHeight = static_cast<int>(buttonWidth * 0.35f);

    m_exitSprite = std::make_unique<CSprite>(
        buttonWidth,
        buttonHeight,
        "assets/texture/exit.png");

    m_startSprite = std::make_unique<CSprite>(
        buttonWidth,
        buttonHeight,
        "assets/texture/start.png");

    const auto& s = ConnectionSettingsStore::Get();
    m_myPortInput = s.myPort;
	m_hostMyPortInput = s.myPort;
	m_joinMyPortInput = s.myPort;
    m_remotePortInput = s.remotePort;
    std::fill(m_remoteIpInput.begin(), m_remoteIpInput.end(), '\0');
    strncpy_s(
        m_remoteIpInput.data(),
        m_remoteIpInput.size(),
        s.remoteIp.c_str(),
        _TRUNCATE);

    //Matching用入力の初期化
	//Server IP/Portを分けて所持し、BuildServerUrl()でMtchingClientに渡す
    std::fill(m_serverIpInput.begin(), m_serverIpInput.end(), '\0');
    strncpy_s(
        m_serverIpInput.data(),
        m_serverIpInput.size(),
        s.matchingServerIp.c_str(),
        _TRUNCATE);
	m_serverPortInput = s.matchingServerPort;

	std::fill(m_roomIdInput.begin(), m_roomIdInput.end(), '\0');
    strncpy_s(
        m_roomIdInput.data(),
        m_roomIdInput.size(),
		s.roomId.c_str(),
		_TRUNCATE);
    m_hostToken = s.hostToken;
	m_statusMessage = "idle";
	m_errorMessage.clear();
	m_lastMatchedEndpoint = {};
	m_matchingUIState = MatchingUIState::Idle;
    m_pollAccumMs = 0;
	m_waitAccumMs = 0;
}

void TitleScene::dispose()
{
    if (m_startSprite) {
        m_startSprite->Dispose();
        m_startSprite.reset();
    }

    if (m_exitSprite) {
        m_exitSprite->Dispose();
        m_exitSprite.reset();
    }

    if (m_backgroundSprite) {
        m_backgroundSprite->Dispose();
        m_backgroundSprite.reset();
    }
}