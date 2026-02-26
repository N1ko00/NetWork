#include "ResultScene.h"
#include "system/scenemanager.h"
#include "system/CDirectInput.h"
#include "system/DebugUI.h"

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

    ImGui::Begin("Result");
    ImGui::Text("YOU LOSE");
    ImGui::Text("Press Enter to retry");
    ImGui::End();
}

void ResultScene::init()
{
}

void ResultScene::dispose()
{
}