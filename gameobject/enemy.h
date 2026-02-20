#pragma once

#include	<memory>
#include	"gameobject.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/CShader.h"
#include	"../system/IScene.h"

class enemy : public gameobject {

public:

	enemy(IScene* currentscene)
		: m_meshrenderer(nullptr),
		m_mesh(nullptr),
		m_shader(nullptr),
		gameobject(currentscene) {
	}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	// 動きのパラメータ
	const float VALUE_MOVE_MODEL = 2.0f;					// 移動速度
	const float VALUE_ROTATE_MODEL = PI * 0.02f;			// 回転速度
	const float RATE_ROTATE_MODEL = 0.40f;					// 回転慣性係数
	const float RATE_MOVE_MODEL = 0.20f;					// 移動慣性係数

private:
	CStaticMesh*			m_mesh;
	CStaticMeshRenderer*	m_meshrenderer;
	CShader*				m_shader;

	// スピード
	float m_speed = 0.1f;	// 移動速度

	// 移動量
	Vector3	m_move = { 0.0f,0.0f,0.0f };
	// 目標回転角度
	Vector3	m_destrot = { 0.0f,0.0f,0.0f };
};