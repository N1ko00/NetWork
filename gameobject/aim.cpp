#include	"aim.h"	
#include	"../system/commontypes.h"
#include	"../system/meshmanager.h"

void aim::init() {

	float u[4] = {	0.125f * 6, 
					0.125f * 6, 
					0.125f * 7, 
					0.125f * 7 };
	float v[4] = {	0.25f, 
					0.0f, 
					0.25f, 
					0.0f };

	{
		VERTEX_3D vtx{};

		vtx.Position.x = -m_width / 2.0f;
		vtx.Position.y =  m_height;
		vtx.Position.z =  0.0f;
		vtx.Diffuse = Color(1, 1, 1, 1);
		vtx.Normal = Vector3(0, 0, -1);
		vtx.TexCoord = Vector2(u[1], v[1]);

		m_vertices.push_back(vtx);

		vtx.Position.x =  m_width / 2.0f;
		vtx.Position.y =  m_height;
		vtx.Position.z =  0.0f;
		vtx.Diffuse = Color(1, 1, 1, 1);
		vtx.Normal = Vector3(0, 0, -1);
		vtx.TexCoord = Vector2(u[3], v[3]);
		m_vertices.push_back(vtx);

		vtx.Position.x = -m_width / 2.0f;
		vtx.Position.y =  0.0f;
		vtx.Position.z =  0.0f;
		vtx.Diffuse = Color(1, 1, 1, 1);
		vtx.Normal = Vector3(0, 0, -1);
		vtx.TexCoord = Vector2(u[0], v[0]);
		m_vertices.push_back(vtx);

		vtx.Position.x =  m_width / 2.0f;
		vtx.Position.y = 0.0f;
		vtx.Position.z = 0.0f;
		vtx.Diffuse = Color(1, 1, 1, 1);
		vtx.Normal = Vector3(0, 0, -1);
		vtx.TexCoord = Vector2(u[2], v[2]);
		m_vertices.push_back(vtx);

	}

	// 頂点バッファ生成
	m_VertexBuffer.Create(m_vertices);

	// シェーダオブジェクト生成
	m_shader = MeshManager::getShader<CShader>("unlightshader");

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
	bool sts = m_Texture.Load(std::filesystem::path("assets/texture/UI64x64.png"));
	assert(sts == true);

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1, 1, 1);
	m_srt.rot = Vector3(0, 0, 0);

}

void aim::update(uint64_t dt) {

}

void aim::draw(uint64_t dt) {

	Matrix4x4 mtx = Matrix4x4::Identity;

	Renderer::SetWorldMatrix(&mtx);

	// 描画の処理
	ID3D11DeviceContext* devicecontext;
	devicecontext = Renderer::GetDeviceContext();

	// トポロジーをセット（旧プリミティブタイプ）
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_shader->SetGPU();
	m_VertexBuffer.SetGPU();

	m_Material.SetGPU();
	m_Texture.SetGPU();

	devicecontext->Draw(
		4,
		0);
}

void aim::drawbillboard(Camera* camera) {
	
	Matrix4x4 viewmtx = camera->GetViewMatrix();

	// 転置行列作成
	Matrix4x4 trasnposemtx = viewmtx.Transpose();

	Matrix4x4 billboardmtx = Matrix4x4::Identity;
	billboardmtx._11 = trasnposemtx._11;
	billboardmtx._12 = trasnposemtx._12;
	billboardmtx._13 = trasnposemtx._13;

	billboardmtx._21 = trasnposemtx._21;
	billboardmtx._22 = trasnposemtx._22;
	billboardmtx._23 = trasnposemtx._23;

	billboardmtx._31 = trasnposemtx._31;
	billboardmtx._32 = trasnposemtx._32;
	billboardmtx._33 = trasnposemtx._33;

	billboardmtx._41 = m_srt.pos.x;
	billboardmtx._42 = m_srt.pos.y;
	billboardmtx._43 = m_srt.pos.z;

	Renderer::SetWorldMatrix(&billboardmtx);

	// 描画の処理
	ID3D11DeviceContext* devicecontext;
	devicecontext = Renderer::GetDeviceContext();

	// トポロジーをセット（旧プリミティブタイプ）
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_shader->SetGPU();
	m_VertexBuffer.SetGPU();

	m_Material.SetGPU();
	m_Texture.SetGPU();

	devicecontext->Draw(
		4,
		0);

}

void aim::dispose() {

}
