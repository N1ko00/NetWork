#include	"../scene/P2PScene.h"
#include    "../system/CDirectInput.h"
#include	"../system/meshmanager.h"
#include	"enemy.h"	


void enemy::init() 
{
	m_mesh = MeshManager::getMesh<CStaticMesh>("car001.x");
	m_shader = MeshManager::getShader<CShader>("lightshader");
	m_meshrenderer=MeshManager::getRenderer<CStaticMeshRenderer>("car001.x");

	m_srt.rot = Vector3(0, 0, PI);

}

void enemy::update(uint64_t dt) {


}

bool enemy::CheckHitByBullet(const Vector3& bulletPos)
{
	if (!m_isAlive) {
		return false;
	}

	const Vector3 center = m_srt.pos + Vector3(0.0f, 10.0f, 0.0f);
	const Vector3 diff = bulletPos - center;
	const float radiusSq = m_collisionRadius * m_collisionRadius;
	if (diff.LengthSquared() <= radiusSq) {
		m_isAlive = false;
		return true;
	}

	return false;
}

void enemy::draw(uint64_t dt) 
{
	if (!m_isAlive) {
		return;
	}

	Matrix4x4 mtx = m_srt.GetMatrix();

	Renderer::SetWorldMatrix(&mtx);

	std::cout << "pos: " << mtx._41 << "," << mtx._42 << "," << mtx._43 << std::endl;

	m_shader->SetGPU();
	m_meshrenderer->Draw();
}

void enemy::dispose() {

}
