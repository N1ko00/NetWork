#pragma once

#include "system/IScene.h"
#include "system/SceneClassFactory.h"
#include "system/CSprite.h"

#include <memory>
#include <array>
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

    int m_myPortInput = 50001;
    int m_remotePortInput = 50000;
    std::array<char, 64> m_remoteIpInput{};
	std::array<char, 128> m_serverUrlInput{};
	std::array<char, 64> m_roomIdInput{};
    std::string m_hostToken{};
    std::string m_matchStatus{};
	MatchingClient m_matchingClient;
    bool m_isHostWaiting = false;
	uint64_t m_pollAccumMs = 0;
	uint64_t m_pollIntervalMs = 1000;
    
    bool ApplyAndGoToP2PScene();  
	bool ApplyRemoteAndGoToP2PScene(const std::string& remoteIp, int remotePort, int myPort);
    void UpdateHostPolling(uint64_t delta);
};

REGISTER_CLASS(TitleScene)