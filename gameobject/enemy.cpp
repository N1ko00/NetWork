#include	"../scene/P2PScene.h"
#include    "../system/CDirectInput.h"
#include	"../system/meshmanager.h"
#include	"enemy.h"	


void enemy::init() 
{
	m_mesh = MeshManager::getMesh<CStaticMesh>("car002.x");
	m_shader = MeshManager::getShader<CShader>("lightshader");
	m_meshrenderer = MeshManager::getRenderer<CStaticMeshRenderer>("car002.x");
}

void enemy::update(uint64_t dt) {


}

void enemy::draw(uint64_t dt) 
{

	Matrix4x4 mtx = m_srt.GetMatrix();

	Renderer::SetWorldMatrix(&mtx);

	std::cout << "pos: " << mtx._41 << "," << mtx._42 << "," << mtx._43 << std::endl;

	m_shader->SetGPU();
	m_meshrenderer->Draw();

}

void enemy::dispose() {

}
