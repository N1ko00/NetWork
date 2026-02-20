#pragma once

#include	"../system/commontypes.h"
#include	"../system/utility.h"

// パーティクル
struct PARTICLE {
	Vector3	pos;			// 位置
	Vector3	velocity;		// 現在速度		
	Vector3	acceleration;	// 加速度		
	int life = 0;			// 寿命
	bool isAlive = false;	// 生存フラグ
};

// パーティクルエミッタ
class Emitter
{
	inline static constexpr unsigned int MAXPARTICLENUM = 1000;		// 物理的上限

	std::vector<PARTICLE> m_particles;		// パーティクル群

	Vector3 m_pos{};						// 位置
	Vector3 m_direction;					// 方向

	Matrix4x4 m_directionMtx;				// パーティクルの射出方向を表す行列（位置と方向）

	// 円錐ベースで定義
	float m_radius = 0.0f;					// パーティクルの発生範囲
	float m_height = 0.0f;					// パーティクルの発生範囲
	unsigned int m_particluenum = 0;		// 発生させるパーティクルの数

	unsigned int m_perframeparticle = 0;	// 1フレームで表示させたいパーティクル数

	unsigned int m_alivecount = 0;			// 生きてるパーティクル数

	// パーティクルのMAX寿命
	int m_maxlife = 100;

	float m_gravity = 0.1f;			// 重力加速度

public:
	// 開始
	void Start(
		Vector3 pos,				// エミッターの位置			
		unsigned int num,			// 1フレームで表示させたい数
		float radius,				// 円錐の半径
		float height,				// 円錐の高さ
		float azimuth,				// 円錐の方向（方位角）
		float elevation,			// 円錐の方向（仰角）
		int maxlife,				// 寿命
		float gravity);				// 重力

	// 更新
	void Update();

	const std::vector<PARTICLE>& GetParticles();

};
