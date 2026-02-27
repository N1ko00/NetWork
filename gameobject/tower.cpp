#include	"tower.h"	
#include	"../system/meshmanager.h"

void tower::init() {

	m_mesh = MeshManager::getMesh<CStaticMesh>("Tower");
	m_meshrenderer = MeshManager::getRenderer<CStaticMeshRenderer>("Tower");
	m_shader = MeshManager::getShader<CShader>("unlightshader");

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(0.1f, 0.1f, 0.1f);
	m_srt.rot = Vector3(0, 0, 0);
}

void tower::update(uint64_t dt) {

}

void tower::draw(uint64_t dt) {


	Matrix4x4 mtx = m_srt.GetMatrix();

	Renderer::SetWorldMatrix(&mtx);

	m_shader->SetGPU();
	m_meshrenderer->Draw();

}

void tower::dispose() {

}
