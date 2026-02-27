#pragma once

#include "system/IScene.h"
#include "system/SceneClassFactory.h"
#include "system/CSprite.h"

#include <memory>

class ResultScene : public IScene {
public:
    ResultScene() = default;

    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void init() override;
    void dispose() override;

    Camera* GetCamera() override { return nullptr; }
    uint64_t getmachineid() override { return 0; }

private:
    std::unique_ptr<CSprite> m_backgroundSprite;
    std::unique_ptr<CSprite> m_loseSprite;
    std::unique_ptr<CSprite> m_returnTitleSprite;
    std::unique_ptr<CSprite> m_exitSprite;
};

REGISTER_CLASS(ResultScene)