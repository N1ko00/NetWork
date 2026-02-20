#include	<iostream>
#include	"CCylinderMesh.h"

/**
 * @brief 円柱メッシュの初期化処理
 *
 * 指定された分割数・半径・高さ・色に基づいて円柱の頂点とインデックスを生成します。
 *
 * @param divx 円周方向の分割数
 * @param radius 円柱の半径
 * @param height 円柱の高さ
 * @param color 頂点カラー
 */
void CCylinderMesh::Init(
	int		divx,
	float	radius,
	float	height,
	Color color)
{
	// サイズセット（幅と高さ）（XY平面）
	m_height = height;
	m_width = 2.0f * PI * radius;		// 直径×円周率
	m_radius = radius;

	// 分割数
	m_division_x = divx;

	// 頂点カラー
	m_color = color;

	// 頂点データ生成
	CreateVertex();
}

/**
 * @brief 円柱メッシュの頂点とインデックスデータを生成する
 *
 * 側面、上面、底面を構成するすべての頂点・インデックスを計算し `m_vertices` および `m_indices` に格納します。
 */
void CCylinderMesh::CreateVertex() {

	// 頂点データクリア
	m_vertices.clear();

	// 底面の円周上の頂点データを作成
	float azimuth = 0.0f;			// 方位角

	// 側面の頂点データ（底面）
	for (unsigned int i = 0; i <= m_division_x; i++) {
		azimuth = (2 * PI * (float)i) / (float)m_division_x;	// 方位角をセット
		VERTEX_3D	v{};

		v.Position.x = m_radius * cosf(azimuth);
		v.Position.y = 0.0f;
		v.Position.z = m_radius * sinf(azimuth);

		v.Normal = v.Position;
		v.Normal.Normalize();			// 法線ベクトルを正規化	
		v.Diffuse = m_color;

		m_vertices.emplace_back(v);
	}

	// 側面の頂点データ（上面）
	for (unsigned int i = 0; i <= m_division_x; i++) {
		azimuth = (2 * PI * (float)i) / (float)m_division_x;	// 方位角をセット
		VERTEX_3D	v{};

		v.Position.x = m_radius * cosf(azimuth);
		v.Position.y = m_height;
		v.Position.z = m_radius * sinf(azimuth);

		v.Normal = Vector3(0.0f, 1.0f, 0.0f);	// 法線は上方向
		v.Diffuse = m_color;

		m_vertices.emplace_back(v);
	}

	// 側面（三角形ストリップ）インデックス（下段）
	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.push_back(i);
		m_indices.push_back(i + m_division_x + 2);
		m_indices.push_back(i + 1);
	}

	// 側面（三角形ストリップ）インデックス（上段）
	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.push_back(i);
		m_indices.push_back(i + m_division_x + 1);
		m_indices.push_back(i + m_division_x + 2);
	}

	// 底面中心の頂点を追加
	{
		VERTEX_3D v{};
		v.Position = Vector3(0.0f, 0.0f, 0.0f);
		v.Normal = Vector3(0.0f, 1.0f, 0.0f);
		v.Diffuse = m_color;
		m_vertices.emplace_back(v);
	}

	// 底面の中心インデックス
	unsigned int bottomcenteridx = static_cast<unsigned int>(m_vertices.size() - 1);

	// 底面の三角形インデックス
	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.push_back(bottomcenteridx);
		m_indices.push_back(i);
		m_indices.push_back(i + 1);
	}

	// 上面中心の頂点を追加
	{
		VERTEX_3D v{};
		v.Position = Vector3(0.0f, m_height, 0.0f);
		v.Normal = Vector3(0.0f, 1.0f, 0.0f);
		v.Diffuse = m_color;
		m_vertices.emplace_back(v);
	}

	// 上面の中心インデックス
	int topcenteridx = static_cast<int>(m_vertices.size() - 1);

	// 上面の三角形インデックス
	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.push_back(topcenteridx);
		m_indices.push_back(i + m_division_x + 2);
		m_indices.push_back(i + m_division_x + 1);
	}
}
