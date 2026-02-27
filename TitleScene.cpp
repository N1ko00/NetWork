#include "TitleScene.h"

#include "Application.h"
#include "system/CDirectInput.h"
#include "system/scenemanager.h"

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

    if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
        SceneManager::SetCurrentScene("P2PScene");
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
            SceneManager::SetCurrentScene("P2PScene");
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