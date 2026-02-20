#pragma once

#include	<memory>
#include	"gameobject.h"
#include	"../system/CShader.h"
#include    "../system/CIndexBuffer.h"
#include    "../system/CVertexBuffer.h"
#include	"../system/CMaterial.h"
#include	"../system/CTexture.h"
#include	"../system/CPlane.h"
#include    "../system/collision.h"
#include    "../system/camera.h"

class aim : public gameobject {
public:
	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	struct Face {					// 三角形のインデックスデータ
		int		idx[3];
	};

	void drawbillboard(Camera*);

private:
	float m_width = 100.0f;			// 幅
	float m_height = 100.0f;		// 高さ

	// 描画の為の情報（メッシュに関わる情報）
	CVertexBuffer<VERTEX_3D>	m_VertexBuffer;				// 頂点バッファ

	// 描画の為の情報（見た目に関わる部分）
	CShader*					m_shader;					// シェーダー
	CMaterial					m_Material;					// マテリアル
	CTexture					m_Texture;					// テクスチャ

	std::vector<VERTEX_3D>		m_vertices;					// 頂点群
	std::vector<uint32_t>		m_indices;					// インデックス群

};