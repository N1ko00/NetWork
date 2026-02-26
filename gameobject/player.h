#pragma once

#include	<memory>
#include    <vector>
#include	"gameobject.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/CShader.h"

class player : public gameobject {

public:
	player(IScene* currentscene)
		: m_meshrenderer(nullptr),
		m_mesh(nullptr),
		m_shader(nullptr),
		gameobject(currentscene) {
	}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	struct Bullet {
		Vector3 pos;
		Vector3 vel;
		float life = 0.0f;
		bool isLocal = true;
	};

	void SpawnLocalBullet();
	void SpawnNetworkBullet(const Vector3& pos, const Vector3& dir);
	std::vector<Bullet>& GetBullets() { return m_bullets; }
	const std::vector<Bullet>& GetBullets() const { return m_bullets; }
	void RemoveBulletAt(std::size_t index);
	bool CheckHitByEnemyBullet(const Vector3& bulletPos) const;
	const std::vector<Bullet>& GetJustFiredBullets() const { return m_justFiredBullets; }
	void ClearJustFiredBullets() { m_justFiredBullets.clear(); }

	// “®‚«‚Ìƒpƒ‰ƒ[ƒ^
	const float VALUE_MOVE_MODEL = 2.0f;			// ˆÚ“®‘¬“x
	const float VALUE_ROTATE_MODEL = PI * 0.02f;	// ‰ñ“]‘¬“x
	const float RATE_ROTATE_MODEL = 0.40f;			// ‰ñ“]Šµ«ŒW”
	const float RATE_MOVE_MODEL = 0.20f;			// ˆÚ“®Šµ«ŒW”

private:
	CStaticMesh*			m_mesh;
	CStaticMeshRenderer*	m_meshrenderer;
	CShader*				m_shader;

	// ˆÚ“®—Ê
	Vector3	m_move = { 0.0f,0.0f,0.0f };
	// –Ú•W‰ñ“]Šp“x
	Vector3	m_destrot = { 0.0f,0.0f,0.0f };


	std::vector<Bullet> m_bullets;
	std::vector<Bullet> m_justFiredBullets;
};