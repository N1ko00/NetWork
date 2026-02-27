#pragma once

#include	<memory>
#include	"gameobject.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/CShader.h"
#include	"../system/IScene.h"

class tower : public gameobject {

public:
	tower(IScene* currentscene)
		: m_meshrenderer(nullptr),
		m_mesh(nullptr),
		m_shader(nullptr),
		gameobject(currentscene) {
	}
	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

private:
	CStaticMesh*			m_mesh;
	CStaticMeshRenderer*	m_meshrenderer;
	CShader*	m_shader;

};