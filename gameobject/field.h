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

class field : public gameobject {

public:

	field(IScene* currentscene)
		: gameobject(currentscene) {
	}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setwidth(float w){
		m_width = w;
	}
	void setdepth(float z) {
		m_depth = z;
	}
	void setdividex(int divx) {

		m_divX = divx;
	}
	void setdividez(int divz) {
		m_divZ = divz;
	}

	void makeundulationwithrandom(float min,float max);
	void makeundulationwithperlin(
		float hMin,
		float hMax,
		float scale,
		float offsetx,
		float offsetz);

	struct Face {					// 三角形のインデックスデータ
		int		idx[3];
	};

	float GetHeight(Vector3 pos);	// 全面検索		
	float GetHeight2(Vector3 pos);	// 簡単な枝刈り付き

	// 平面の方程式を生成する
	void MakePlaneEquatation();

	// 指定した位置の矩形インデックス(左下原点)を見つける
	int GetSquareNo(Vector3 pos);

	// 指定した位置矩形内の三角形情報を２個分取得する
	bool GetFace(Vector3 pos, std::array<field::Face, 2>& ret);

	// 指定した三角形の頂点情報を取得する
	void GetFaceVertex(int faceindex, std::array<Vector3, 3>& vertices);

	// 指定したLINE上の位置を返す
	std::vector<Vector3>  GetFieldPositionFromLine(GM31::GE::Collision::Line line);

private:
	float m_width = 1000.0f;			// 幅
	float m_depth = 1000.0f;			// 奥行

	int	m_divX = 10;				// X方向分割数
	int	m_divZ = 10;				// Z方向分割数

	// 描画の為の情報（メッシュに関わる情報）
	CIndexBuffer				m_IndexBuffer;				// インデックスバッファ
	CVertexBuffer<VERTEX_3D>	m_VertexBuffer;				// 頂点バッファ

	// 描画の為の情報（見た目に関わる部分）
	CShader						m_Shader;					// シェーダー
	CMaterial					m_Material;					// マテリアル
	CTexture					m_Texture;					// テクスチャ

	std::vector<VERTEX_3D>		m_vertices;					// 頂点群
	std::vector<uint32_t>		m_indices;					// インデックス群

	void CreateVertexXZ();									// XZ平面頂点作成
	void CreateIndexCW();									// インデックス生成

	std::vector<CPlane>			m_planequataions;			// 平面の方程式群

};