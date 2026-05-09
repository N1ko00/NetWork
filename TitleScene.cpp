#include "TitleScene.h"

#include "Application.h"
#include "system/CDirectInput.h"
#include "system/scenemanager.h"
#include "system/imgui/imgui.h"
#include <cstring>
#include <algorithm>
#include <iostream>

namespace {
    bool IsValidPort(int p) { return p >= 0 && p <= 65535; }
}

bool TitleScene::ApplyAndGoToP2PScene()
{
    if (!IsValidPort(m_myPortInput) || !IsValidPort(m_remotePortInput)) return false;
    if (m_remoteIpInput[0] == '\0') return false;
    
    auto & s = ConnectionSettingsStore::Mutable();
    s.enabled = true;
    s.myPort = m_myPortInput;
    s.remotePort = m_remotePortInput;
    s.remoteIp = m_remoteIpInput.data();
    SceneManager::SetCurrentScene("P2PScene");
    return true;
}

bool TitleScene::ApplyRemoteAndGoToP2PScene(const std::string& remoteIp, int remotePort, int myPort) {
	if (!IsValidPort(myPort) || !IsValidPort(remotePort) || remoteIp.empty()) return false;
	//ConectionSettingsStoreに保存
	//マッチングで取得したendpointをp2pのruntime settingに適用する
	auto& s = ConnectionSettingsStore::Mutable();
	s.enabled = true;
	s.myPort = myPort;
	s.remotePort = remotePort;
	s.remoteIp = remoteIp;
	SceneManager::SetCurrentScene("P2PScene");
	return true;
}

void TitleScene::UpdateHostPolling(uint64_t delta){
    if (!m_isHostWaiting)return;
	m_pollAccumMs += delta / 1000; //usec -> msec 想定
	if (m_pollAccumMs < m_pollAccumMs)return;
	m_pollAccumMs = 0;

	//マッチングサーバにpollする
	const std::string sercerurl = m_serverUrlInput.data();
    const std::string roomId = m_roomIdInput.data();
	auto poll = m_matchingClient.PollRoom(sercerurl, roomId, m_hostToken);
    if(!poll.ok) {
        m_matchStatus = "Poll error: " + poll.error;
        return;
	}
    if (!poll.matched) {
        m_matchStatus = "waiting join...";
        return;
    }

	//マッチング成立
    m_matchStatus = "matched. go P2P";
    m_isHostWaiting = false;
    ApplyRemoteAndGoToP2PScene(poll.joinEndpoint.ip, poll.joinEndpoint.port, m_myPortInput);
}


void TitleScene::update(uint64_t delta)
{
    (void)delta;

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

    ImGui::Begin("P2P Connection Setup");
    //既存の手動入力
    ImGui::InputInt("My Port", &m_myPortInput);
    ImGui::InputText("Remote IP", m_remoteIpInput.data(), static_cast<int>(m_remoteIpInput.size()));
    ImGui::InputInt("Remote Port", &m_remotePortInput);
    ImGui::Text("Current: myport=%d remote=%s:%d", m_myPortInput, m_remoteIpInput.data(), m_remotePortInput);
    
    //追加の入力
    if (ImGui::Button("Manual Connect")) {
        if (!ApplyAndGoToP2PScene()) {
			m_matchStatus = "manual connect failed. invalid input";
        }
    }
	ImGui::Separator();
	ImGui::InputText("Server URL", m_serverUrlInput.data(), static_cast<int>(m_serverUrlInput.size()));
	ImGui::InputText("Room ID", m_roomIdInput.data(), static_cast<int>(m_roomIdInput.size()));

    if (ImGui::Button("Host(Create)")) {
		//マッチングサーバーにホスト登録する
		auto r = m_matchingClient.CreateRoom(m_serverUrlInput.data(), m_myPortInput);
        if (!r.ok) {
			m_matchStatus = "Create failed :" + r.error;
			m_isHostWaiting = false;
        }
        else {
			std::fill(m_roomIdInput.begin(), m_roomIdInput.end(), '\0');

        }
    }
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
    m_remotePortInput = s.remotePort;
    std::fill(m_remoteIpInput.begin(), m_remoteIpInput.end(), '\0');
    strncpy_s(
        m_remoteIpInput.data(),
        m_remoteIpInput.size(),
        s.remoteIp.c_str(),
        _TRUNCATE);
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