#pragma once

#include "system/IScene.h"
#include "system/SceneClassFactory.h"

class ResultScene : public IScene {
public:
    ResultScene() = default;

    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void init() override;
    void dispose() override;

    Camera* GetCamera() override { return nullptr; }
    uint64_t getmachineid() override { return 0; }
};

REGISTER_CLASS(ResultScene)