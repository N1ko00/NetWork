#pragma once
#include <cstdint>

// 前方参照
class Camera;

// シーンインタフェース
class IScene {
public:
	IScene() = default;
	virtual ~IScene() = default;
	virtual void update(uint64_t delta) = 0;
	virtual void draw(uint64_t delta) = 0;
	virtual void init() = 0;
	virtual void dispose() = 0;

	virtual Camera* GetCamera() = 0;
	virtual uint64_t getmachineid() = 0;
};