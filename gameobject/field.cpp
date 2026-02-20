#include	"field.h"
#include	"../system/stb_perlin.h"
#include	"../system/RandomEngine.h"
#include	"../system/collision.h"
#include	"../utility/ScreenToWorld.h"

void field::CreateVertexXZ() {
	// 頂点データクリア
	m_vertices.clear();

	for (int z = 0; z <= m_divZ; z++) {
		VERTEX_3D	vtx{};

		for (int x = 0; x <= m_divX; x++) {

			// 頂点座標セット
			vtx.Position.x = -m_width / 2.0f + x * m_width / m_divX;
			vtx.Position.y = 0.0f;
			vtx.Position.z = -m_depth / 2.0f + z * m_depth / m_divZ;

			// 法線ベクトルセット
			vtx.Normal = Vector3(0,1,0);				// 法線をセット

			vtx.Diffuse = Color(1,1,1,1);				// カラー値セット

			// 繰り返しに対応（テクスチャの）
			float texu = 1.0f * m_divX;
			float texv = 1.0f * m_divZ;
			vtx.TexCoord.x = (texu * x / m_divX);
			vtx.TexCoord.y = (texv * z / m_divZ);

			m_vertices.emplace_back(vtx);		// 頂点データセット
		}
	}
}

void field::CreateIndexCW() {
	// インデックスデータクリア
	m_indices.clear();

	// インデックス生成
	for (int z = 0; z < m_divZ; z++) {
		for (int x = 0; x < m_divX; x++) {
			int count = (m_divX + 1) * z + x;		// 左下座標のインデックス

			// 下半分
			{
				Face face{};

				face.idx[0] = count;						// 左下
				face.idx[1] = count + 1 + (m_divX + 1);		// 右上
				face.idx[2] = count + 1;					// 右

				m_indices.emplace_back(face.idx[0]);
				m_indices.emplace_back(face.idx[1]);
				m_indices.emplace_back(face.idx[2]);

			}

			// 上半分
			{
				Face face{};
				face.idx[0] = count;						// 左下
				face.idx[1] = count + (m_divX + 1);			// 上
				face.idx[2] = count + (m_divX + 1) + 1;		// 右上

				m_indices.emplace_back(face.idx[0]);
				m_indices.emplace_back(face.idx[1]);
				m_indices.emplace_back(face.idx[2]);

			}
		}
	}
}

void field::init() {

	// 頂点データ作成
	CreateVertexXZ();
	// インデックスデータ作成
	CreateIndexCW();

	// 頂点バッファ生成
	m_VertexBuffer.Create(m_vertices);
	// インデックスバッファ生成
	m_IndexBuffer.Create(m_indices);

	// シェーダオブジェクト生成
	m_Shader.Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");

	// マテリアル生成
	MATERIAL	mtrl;
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = TRUE;

	m_Material.Create(mtrl);

	// テクスチャロード
	bool sts = m_Texture.Load(std::filesystem::path("assets/texture/Grass01.jpg"));
	assert(sts == true);

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1, 1, 1);
	m_srt.rot = Vector3(0, 0, 0);

	// 平面の方程式を生成（全面）
	MakePlaneEquatation();
}

void field::update(uint64_t dt) {

}

void field::draw(uint64_t dt) 
{

	Matrix4x4 mtx = Matrix4x4::Identity;

	Renderer::SetWorldMatrix(&mtx);

	// 描画の処理
	ID3D11DeviceContext* devicecontext;
	devicecontext = Renderer::GetDeviceContext();

	// トポロジーをセット（旧プリミティブタイプ）
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();

	m_Material.SetGPU();
	m_Texture.SetGPU();

	devicecontext->DrawIndexed(
		static_cast<int>(m_indices.size()),			// 描画するインデックス数
		0,											// 最初のインデックスバッファの位置
		0);
}

void field::dispose() {

}

void field::makeundulationwithrandom(float min,float max)
{
	RandomEngine rnd;
	rnd.tls();
	
	for (auto& v : m_vertices) {
		v.Position.y = static_cast<float>(rnd.uniformReal(static_cast<double>(min),static_cast<double>(max)));
	}

	for (int trianglenum = 0; trianglenum < m_indices.size() / 3;trianglenum++) {

		int idx0 = m_indices[trianglenum * 3 + 0];
		int idx1 = m_indices[trianglenum * 3 + 1];
		int idx2 = m_indices[trianglenum * 3 + 2];

		Vector3 normal;
		Vector3 edge01 = m_vertices[idx1].Position - m_vertices[idx0].Position;
		Vector3 edge02 = m_vertices[idx2].Position - m_vertices[idx0].Position;

		normal = edge01.Cross(edge02);
		normal.Normalize();

		m_vertices[idx0].Normal = normal;
		m_vertices[idx1].Normal = normal;
		m_vertices[idx2].Normal = normal;
	}

	// 頂点バッファ再作成
	m_VertexBuffer.Modify(m_vertices);

	// 平面の方程式を生成（全面）
	MakePlaneEquatation();
}

void field::makeundulationwithperlin(
	float min, 
	float max,
	float scale,
	float offsetx,
	float offsetz)
{
	const int width = m_divX + 1;
	const int height = m_divZ + 1;

	for (int i = 0; i < static_cast<int>(m_vertices.size()); ++i)
	{
		// 1D -> 2D
		int x = i % width;     // ← 幅で割る
		int z = i / height;     // ← 高さで割る

		// 浮動小数にしてスケール＆オフセット
		float fx = x * scale + offsetx;
		float fz = z * scale + offsetz;

		// 2Dノイズ（y=0に固定）
		float n = stb_perlin_noise3(fx, 0.0f, fz, 0, 0, 0); // [-1,1]

		// [-1,1] -> [0,1] -> [hMin, hMax]
		float h01 = (n + 1.0f) * 0.5f;
		float h = min + (max - min) * h01;

		m_vertices[i].Position.y = h;
	}

	for (int trianglenum = 0; trianglenum < m_indices.size() / 3; trianglenum++) {

		int idx0 = m_indices[trianglenum * 3 + 0];
		int idx1 = m_indices[trianglenum * 3 + 1];
		int idx2 = m_indices[trianglenum * 3 + 2];

		Vector3 normal;
		Vector3 edge01 = m_vertices[idx1].Position - m_vertices[idx0].Position;
		Vector3 edge02 = m_vertices[idx2].Position - m_vertices[idx0].Position;

		normal = edge01.Cross(edge02);
		normal.Normalize();

		m_vertices[idx0].Normal = normal;
		m_vertices[idx1].Normal = normal;
		m_vertices[idx2].Normal = normal;
	}

	m_VertexBuffer.Modify(m_vertices); // 頂点バッファ更新

	// 平面の方程式を生成（全面）
	MakePlaneEquatation();
}

// 現在位置の高さ情報を取得する（全面検索）
float field::GetHeight(Vector3 pos)
{
	// ３角形数分ループ
	for (int trianglenum = 0; trianglenum < m_indices.size() / 3; trianglenum++) {
		Vector3 up = { 0,1,0 };							// 上向きベクトル
		Vector3 startpoint = { pos.x,0,pos.z };			// 直線上の点
		Vector3 ans;									// 交点（平面と直線の交点）
		float t = 0.0f;									// 媒介変数

		PLANEINFO pinfo = m_planequataions[trianglenum].GetPlaneInfo();				// あらかじめ作成していた平面のデータを取得

		// 交点を求める
		bool sts = GM31::GE::Collision::LinetoPlaneCross(pinfo.plane, startpoint, up, t, ans);
		// 交点が存在していた場合
		if (sts) {
			// 内外判定
			bool sts = GM31::GE::Collision::CheckInTriangle(
				pinfo.p0,
				pinfo.p1,
				pinfo.p2,
				ans);
			// 内側だった場合
			if (sts) {
				return ans.y;							// 高さを戻す
			}
		}
	}

	// 見つからなかった場合は高さ０
	return 0;
}

// 現在位置の高さ情報を取得する（簡単な枝刈り付き）
float field::GetHeight2(Vector3 pos)
{
	std::array<field::Face, 2> retfaces;			// 矩形を構成する三角形構成頂点インデックスを２個分
	std::array<Vector3, 3> vertices1;				// 矩形を構成する三角形１構成頂点３個
	std::array<Vector3, 3> vertices2;				// 矩形を構成する三角形２構成頂点３個

	int sqno = GetSquareNo(pos);					// 現在位置から矩形インデックスを取得する
	// 見つかった場合（範囲内だった場合）
	if (sqno != -1) {
		GetFace(pos, retfaces);						// 現在位置から矩形インデックス取得
		GetFaceVertex(sqno * 2, vertices1);			// 矩形を構成する三角形１の頂点情報取得
		GetFaceVertex(sqno * 2 + 1, vertices2);		// 矩形を構成する三角形２の頂点情報取得

		// 内外判定
		bool sts = GM31::GE::Collision::CheckInTriangle(
			vertices1[0],							// ３角形の頂点０
			vertices1[1],							// ３角形の頂点１
			vertices1[2],							// ３角形の頂点２
			pos);									// チェックしたい点

		if (sts) {									// 内側だった場合
			// 平面の方程式生成
			CPlane pl;
			pl.MakeEquatation(
				vertices1[0],
				vertices1[1],
				vertices1[2]);

			float t = 0.0f;
			Vector3 ans;

			// 交点を求める
			bool sts = GM31::GE::Collision::LinetoPlaneCross(
				pl.GetPlaneInfo().plane,
				pos, Vector3(0, 1, 0), t, ans);

			return ans.y;							// 高さを返す
		}
		else {
			// 平面の方程式生成
			CPlane pl;
			pl.MakeEquatation(
				vertices2[0],
				vertices2[1],
				vertices2[2]);

			float t = 0.0f;
			Vector3 ans;

			// 交点を求める
			bool sts = GM31::GE::Collision::LinetoPlaneCross(
				pl.GetPlaneInfo().plane,
				pos, Vector3(0, 1, 0), t, ans);

			return ans.y;
		}
	}
	return 0;		// 見つからなかった場合
}

// 平面の方程式を生成する
void field::MakePlaneEquatation() {

	m_planequataions.clear();

	// 三角形数分ループ
	for (int trianglenum = 0; trianglenum < m_indices.size() / 3; trianglenum++) {

		int idx0 = m_indices[trianglenum * 3 + 0];
		int idx1 = m_indices[trianglenum * 3 + 1];
		int idx2 = m_indices[trianglenum * 3 + 2];

		CPlane p;
		p.MakeEquatation(
			m_vertices[idx0].Position, 
			m_vertices[idx1].Position, 
			m_vertices[idx2].Position);
	
		m_planequataions.emplace_back(p);

	}
}

// 何番目の四角形かを見つける
int field::GetSquareNo(Vector3 pos)
{
	// 範囲チェック
	if (pos.x < -m_width / 2.0f) {
		return -1;
	}
	// 範囲チェック
	if (pos.x > m_width / 2.0f) {
		return -1;
	}
	// 範囲チェック
	if (pos.z > m_depth / 2.0f) {
		return -1;
	}
	// 範囲チェック
	if (pos.z < -m_depth / 2.0f) {
		return -1;
	}

	// 床メッシュはXZ平面をベースで作成している
	double x = pos.x;
	double z = pos.z;

	// 平面の幅
	double planewidth = m_width;

	// 平面の奥行
	double planedepth = m_depth;

	// マップチップサイズ（絶対値で計算）
	double mapchipwidth = fabs(planewidth / m_divX);
	double mapchipheight = fabs(planedepth / m_divZ);

	// 左下原点の相対座標に変換
	double relativex = x + (planewidth / 2.0);
	double relativez = z + (planedepth / 2.0);

	// 左から何番目か？
	unsigned int mapchipx = static_cast<unsigned int>(relativex / mapchipwidth);

	// 下から何番目か？
	unsigned int mapchipz = static_cast<unsigned int>(relativez / mapchipheight);

	// 左下を０番目とした場合の順番を計算
	int squareno;
	squareno = mapchipz * m_divX + mapchipx;

	if (squareno < 0) {
		squareno = 0;
	}
	else {
		if (squareno > static_cast<int>(m_divX) * static_cast<int>(m_divZ) - 1) {
			squareno = m_divX * m_divZ - 1;
		}
	}

	return squareno;
}

// 指定した位置矩形内の三角形情報を２個分取得する
bool field::GetFace(Vector3 pos, std::array<field::Face, 2>& retdata){

	// 現在位置の四角形を取得
	int sqno = GetSquareNo(pos);
	if (sqno == -1) {
		return false;
	}

	int triangleindex1 = sqno*2;
	int triangleindex2 = sqno * 2+1;

	retdata[0].idx[0] = m_indices[triangleindex1 * 3 + 0];
	retdata[0].idx[1] = m_indices[triangleindex1 * 3 + 1];
	retdata[0].idx[2] = m_indices[triangleindex1 * 3 + 2];

	retdata[1].idx[0] = m_indices[triangleindex2 * 3 + 0];
	retdata[1].idx[1] = m_indices[triangleindex2 * 3 + 1];
	retdata[1].idx[2] = m_indices[triangleindex2 * 3 + 2];

	return true;
}

// 指定した三角形インデックスの頂点データを取得する
void field::GetFaceVertex(int triangleindex, std::array<Vector3, 3>& vertices) {

	int idx[3]{};

	idx[0] = m_indices[triangleindex * 3 + 0];
	idx[1] = m_indices[triangleindex * 3 + 1];
	idx[2] = m_indices[triangleindex * 3 + 2];

	vertices[0] = m_vertices[idx[0]].Position;
	vertices[1] = m_vertices[idx[1]].Position;
	vertices[2] = m_vertices[idx[2]].Position;
}

// 指定したマウス座標からフィールド座標を取得する 
std::vector<Vector3>  field::GetFieldPositionFromLine(GM31::GE::Collision::Line line)
{
	std::vector<Vector3> intersectionpoins{};

	// ３角形数分ループ
	for (int trianglenum = 0; trianglenum < m_indices.size() / 3; trianglenum++) {
		Vector3 ans;									// 交点（平面と直線の交点）
		float t = 0.0f;									// 媒介変数

		PLANEINFO pinfo = m_planequataions[trianglenum].GetPlaneInfo();				// あらかじめ作成していた平面のデータを取得

		// 交点を求める
		bool sts = GM31::GE::Collision::LinetoPlaneCross(pinfo.plane, line.point, line.direction, t, ans);
		// 交点が存在していた場合
		if (sts) {
			// 内外判定
			bool sts = GM31::GE::Collision::CheckInTriangle(
				pinfo.p0,
				pinfo.p1,
				pinfo.p2,
				ans);
			// 内側だった場合
			if (sts) {
				intersectionpoins.push_back(ans);
			}
		}
	}

	return intersectionpoins;
}
