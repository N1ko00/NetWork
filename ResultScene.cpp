#include "ResultScene.h"
#include "system/scenemanager.h"
#include "system/CDirectInput.h"
#include "system/DebugUI.h"
#include "Application.h"

void ResultScene::update(uint64_t delta)
{
    (void)delta;

    if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
        SceneManager::SetCurrentScene("P2PScene");
    }
}

void ResultScene::draw(uint64_t delta)
{
    (void)delta;

    if (m_backgroundSprite) {
        m_backgroundSprite->Draw(
            Vector3(1.0f, 1.0f, 1.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(Application::GetWidth() * 0.5f, Application::GetHeight() * 0.5f, 0.0f));
    }

    if (m_loseSprite) {
        m_loseSprite->Draw(
            Vector3(1.0f, 1.0f, 1.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(Application::GetWidth() * 0.5f, Application::GetHeight() * 0.2f, 0.0f));
    }

    ImGui::Begin("Result");
    ImGui::Text("YOU LOSE");
    ImGui::Text("Press Enter to retry");
    ImGui::End();
}

void ResultScene::init()
{
    m_backgroundSprite = std::make_unique<CSprite>(
        static_cast<int>(Application::GetWidth()),
        static_cast<int>(Application::GetHeight()),
        "assets/texture/haikei.jpg");

    const int loseWidth = static_cast<int>(Application::GetWidth() * 0.5f);
    const int loseHeight = static_cast<int>(loseWidth * (1117.0f / 2048.0f));
    m_loseSprite = std::make_unique<CSprite>(
        loseWidth,
        loseHeight,
        "assets/texture/lose.png");
}

void ResultScene::dispose()
{
    if (m_loseSprite) {
        m_loseSprite->Dispose();
        m_loseSprite.reset();
    }

    if (m_backgroundSprite) {
        m_backgroundSprite->Dispose();
        m_backgroundSprite.reset();
    }
}