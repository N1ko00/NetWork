#include	"../scene/P2PScene.h"
#include    "../system/CDirectInput.h"
#include	"../system/meshmanager.h"
#include	"enemy.h"	


void enemy::init() 
{
	m_mesh = MeshManager::getMesh<CStaticMesh>("tank10_base.x");
	m_shader = MeshManager::getShader<CShader>("lightshader");
	m_meshrenderer = MeshManager::getRenderer<CStaticMeshRenderer>("tank10_base.x");
	MeshManager::getMesh<CStaticMesh>("tank10_top.x");
	MeshManager::getMesh<CStaticMesh>("tank10_cat.x");
	MeshManager::getMesh<CStaticMesh>("tank10_pipe.x");
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_top.x");
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_cat.x");
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_pipe.x");
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
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_top.x")->Draw();
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_cat.x")->Draw();
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_pipe.x")->Draw();
}

void enemy::dispose() {

}
