#include "BlobShadow.h"
#include "../system/renderer.h"

BlobShadow::BlobShadow() {}
BlobShadow::~BlobShadow() {}

void BlobShadow::CreateVertexXZ() {
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
            vtx.Normal = Vector3(0, 1, 0);				// 法線をセット

            vtx.Diffuse = Color(1, 1, 1, 1);				// カラー値セット

            // 繰り返しに対応（テクスチャの）
            float texu = 1.0f * m_divX;
            float texv = 1.0f * m_divZ;
            vtx.TexCoord.x = (texu * x / m_divX);
            vtx.TexCoord.y = (texv * z / m_divZ);

            m_vertices.emplace_back(vtx);		// 頂点データセット
        }
    }
}

void BlobShadow::CreateIndexCW() {
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


bool BlobShadow::Init(const char* texturePath)
{
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

    m_Texture.Load(std::filesystem::path(texturePath));

    return true;
}

void BlobShadow::Terminate()
{
    // 必要に応じて解放処理
    // （あなたのエンジンの Destroy/Release 系 API があればそれを呼ぶ）
    // ここでは何もしない想定にしている
}

void BlobShadow::Draw(float radius, float groundY, float ex, float ez)
{
    // 単発描画はデフォルト色を使用
    DrawOne(radius, groundY, ex, ez, m_ShadowColor);
}

void BlobShadow::DrawBatch()
{
    // 登録された全インスタンスを描画
    for (const auto& inst : m_ShadowList)
    {
        DrawOne(inst.radius, inst.groundY, inst.x, inst.z, inst.color);
    }
}

/// 共通の1個描画処理
void BlobShadow::DrawOne(
    float radius,
    float groundY,
    float ex,
    float ez,
    const Color& color
)
{
    // ワールド行列設定（1x1 XZ平面を radius*2 にスケーリング）
    Matrix4x4 mtx = Matrix4x4::CreateScale(radius * 2.0f, 1.0f, radius * 2.0f);

    mtx._41 = ex;
    mtx._42 = groundY + m_HeightOffset;
    mtx._43 = ez;

    Renderer::SetWorldMatrix(&mtx);

    // マテリアル更新（テクスチャ×color）
    m_Material.SetDiffuse(color);
    m_Material.Update();

    m_Shader.SetGPU();
    m_Material.SetGPU();
    m_Texture.SetGPU();

    m_VertexBuffer.SetGPU();
    m_IndexBuffer.SetGPU();

    Renderer::GetDeviceContext()->DrawIndexed(
        static_cast<int>(m_indices.size()),			// 描画するインデックス数
        0,											// 最初のインデックスバッファの位置
        0);

}
