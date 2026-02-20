#include	<random>
#include	"particle.h"
#include	"../system/CPolar3D.h"

void Emitter::Start(
	Vector3 pos,				// エミッタの位置
	unsigned int num,			// 発生させるパーティクルの数
	float radius,				// 円錐の半径
	float height,				// 円錐の高さ
	float azimuth,				// 円錐の方向（方位角）
	float elevation,			// 円錐の方向（仰角）	
	int maxlife,				// 寿命
	float gravity)				// 重力
{
	m_pos = pos;
	m_particluenum = static_cast<unsigned int>(num * 1.5f);				// 配列上限
	m_perframeparticle = num;	// １フレームでアクティブにしておきたいパーティクル数
	m_radius = radius;
	m_height = height;
	m_maxlife = maxlife;
	m_gravity = gravity;

	m_particles.resize(m_particluenum);		// 上限設定
	
	// いったん全て非アクティブ
	for (auto& p : m_particles) {
		p.isAlive = false;
		p.life = 0;
	}

	// 極座標系をオイラー角に変換（パーティクルの射出方向を生成する）
	CPolor3D polar(radius, elevation, azimuth);
	Vector3 targetpos = polar.ToCartesian();

	// パーティクルの射出方向（中心軸）を生成するクオータニオンを作成
	Quaternion quat = utility::CreateTargetQuaternion(
		Vector3(0, 1, 0),
		targetpos);

	// クオータニオンから行列を生成
	m_directionMtx = Matrix4x4::CreateFromQuaternion(quat);

	// 寿命決め
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> distr(1, maxlife);

	// パーティクル初期化		
	std::vector<Vector3> points;
	points = utility::GeneratePointsInsideCone(
			radius,		// 円錐の半径
			height,		// 円錐の高さ
			num);		// 点の数

	// 設定
	int cnt = 0;
	for (unsigned int cnt = 0; cnt < num;cnt++) {
		m_particles[cnt].isAlive = true;
		m_particles[cnt].pos = m_pos;
		m_particles[cnt].velocity = Vector3::Transform(points[cnt], m_directionMtx);
		m_particles[cnt].life = distr(gen);
	}
}

// 更新
void Emitter::Update() {

	// 寿命決め
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> distr(1, m_maxlife);

	m_alivecount = 0;

	for (auto& p : m_particles) {
		p.life--;
		if (p.life <= 0) {
			p.isAlive = false;
			continue;
		}
		m_alivecount++;
		p.pos += p.velocity;
		if (p.pos.y < 0.0f) {
			if (p.velocity.y < 0.0f) {
				p.velocity.y *= - 0.7f;
				p.pos.y = 5.0f;
			}
		}
		p.velocity.y -= m_gravity;		
	}

	// 同時に表示させときたいパーティクル数　　＞　今表示されているパーティクル数
	if(m_perframeparticle > m_alivecount){

		int num = m_perframeparticle - m_alivecount;

		std::vector<Vector3> points = utility::GeneratePointsInsideCone(
			m_radius,					// 半径
			m_height,					// 高さ
			num);						// 個数

		int cnt = 0;

		for (auto& point : points) {
			
			int findindex = -1;
			for (int index = 0; index < m_particles.size();index++) {
				if (m_particles[index].isAlive == false) {
					findindex = index;
					break;
				}
			}
			if (findindex == -1) break;

			m_particles[findindex].pos = m_pos;          // 実際は円錐内など
			m_particles[findindex].velocity = Vector3::Transform(point, m_directionMtx);
			m_particles[findindex].life = distr(gen);
			m_particles[findindex].isAlive = true;
		}

	}

}

const std::vector<PARTICLE>& Emitter::GetParticles() {
	return m_particles;				// パーティクル群
}
