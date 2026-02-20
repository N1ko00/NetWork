#include	<iostream>
#include	"CConeMesh.h"

/**
 * @brief 円錐メッシュを初期化する
 *
 * 指定された分割数、半径、高さ、頂点カラーに基づいて、円錐の頂点・インデックスを生成します。
 * @param divx 横方向の分割数
 * @param radius 円錐底面の半径
 * @param height 円錐の高さ
 * @param color 頂点の色
 * @param bottomup true: 底面→頂点の順で頂点生成、false: 頂点→底面の順で頂点生成
 */
void CConeMesh::Init(
	int		divx,
	float	radius,
	float	height,
	Color color,
	bool bottomup)
{
	// サイズセット（幅と高さ）（XY平面）
	m_height = height;
	m_width = 2.0f * PI * radius;		// 直径×円周率
	m_radius = radius;

	// 分割数
	m_division_x = divx;

	// 頂点カラー
	m_color = color;

	// 頂点デタ生成
	if (bottomup) {
		CreateVertex();
	}
	else {
		CreateVertexTopDown();
	}
}

/**
 * @brief 頂点を「底面→頂点」の順で生成する
 *
 * 円錐の頂点配列を底面から山頂に向かって構成し、インデックスも追加します。
 * 底面周辺の三角形および底面自体のポリゴンも含めて構築します。
 */
void CConeMesh::CreateVertex()
{
	// 頂点データクリア
	m_vertices.clear();

	// てっぺんの頂点	
	VERTEX_3D v{};

	v.Position = Vector3(0.0f, m_height, 0.0f);
	v.Normal = Vector3(0.0f, 1.0f, 0.0f);
	v.Diffuse = m_color;

	m_vertices.emplace_back(v);

	// 底面の円周上の頂点データを作成
	float azimuth = 0.0f;			// 方位角
	for (unsigned int i = 0; i <= m_division_x; i++) {

		azimuth = (2 * PI * (float)i) / (float)m_division_x;	// 方位角をセット

		VERTEX_3D	v{};

		v.Position.x = m_radius * cosf(azimuth);
		v.Position.y = 0.0f;
		v.Position.z = m_radius * sinf(azimuth);

		v.Normal = Vector3(0.0f, 1.0f, 0.0f);		// 法線ベクトルを頂点座標と同じにする	
		v.Diffuse = m_color;

		m_vertices.emplace_back(v);
	}

	// 頂点から底面の頂点へのインデックスを追加
	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.push_back(0);						// 頂点
		m_indices.push_back(i + 2);					// 次の底面頂点
		m_indices.push_back(i + 1);					// 現在の底面頂点
	}

	// 底面の中心座標
	v.Position = Vector3(0.0f, 0.0f, 0.0f);
	v.Normal = Vector3(0.0f, 1.0f, 0.0f);
	v.Diffuse = m_color;
	m_vertices.emplace_back(v);

	size_t bottomcenteridx = m_vertices.size() - 1;	// 底面の中心座標のインデックス

	// 底面の三角形をインデックス化
	for (unsigned int i = 0; i <= m_division_x; i++) {
		m_indices.push_back(static_cast<int>(bottomcenteridx));		// 底面の中心
		m_indices.push_back(i + 1);					// 次の底面頂点
		m_indices.push_back(i + 2);					// 現在の底面頂点
	}
}

/**
 * @brief 頂点を「頂点→底面」の順で生成する
 *
 * 円錐の頂点配列を山頂から底面に向かって構成し、インデックスも追加します。
 * 底面周辺の三角形および底面自体のポリゴンも含めて構築します。
 */
void CConeMesh::CreateVertexTopDown()
{
	// 頂点データクリア
	m_vertices.clear();

	// てっぺんの頂点	
	VERTEX_3D v{};

	v.Position = Vector3(0.0f, 0.0f, 0.0f);
	v.Normal = Vector3(0.0f, 1.0f, 0.0f);
	v.Diffuse = m_color;

	m_vertices.emplace_back(v);

	// 底面の円周上の頂点データを作成
	float azimuth = 0.0f;			// 方位角
	for (unsigned int i = 0; i <= m_division_x; i++) {

		azimuth = (2 * PI * (float)i) / (float)m_division_x;	// 方位角をセット

		VERTEX_3D	v{};

		v.Position.x = m_radius * cosf(azimuth);
		v.Position.y = m_height;
		v.Position.z = m_radius * sinf(azimuth);

		v.Normal = Vector3(0.0f, 1.0f, 0.0f);		// 法線ベクトルを頂点座標と同じにする	
		v.Diffuse = m_color;

		m_vertices.emplace_back(v);
	}

	// 頂点から各セグメントの底面の頂点へのインデックスを追加
	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.push_back(0);						// 頂点
		m_indices.push_back(i + 2);					// 次の底面頂点
		m_indices.push_back(i + 1);					// 現在の底面頂点
	}

	// 底面の中心座標
	v.Position = Vector3(0.0f, m_height, 0.0f);
	v.Normal = Vector3(0.0f, 1.0f, 0.0f);
	v.Diffuse = m_color;
	m_vertices.emplace_back(v);

	size_t bottomcenteridx = m_vertices.size() - 1;	// 底面の中心座標のインデックス

	// 底面の三角形をインデックス化
	for (unsigned int i = 0; i <= m_division_x; i++) {
		m_indices.push_back(static_cast<int>(bottomcenteridx));		// 底面の中心
		m_indices.push_back(i + 1);					// 次の底面頂点
		m_indices.push_back(i + 2);					// 現在の底面頂点
	}
}
