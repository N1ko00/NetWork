#include "CStaticMeshRenderer.h"

void CStaticMeshRenderer::Init(CStaticMesh& mesh)
{
	// 頂点バッファとインデックスバッファを生成
	CMeshRenderer::Init(mesh);

	// サブセット情報取得
	m_Subsets = mesh.GetSubsets();

	// diffuseテクスチャ情報取得
	m_DiffuseTextures = mesh.GetDiffuseTextures();

	// マテリアル情報取得	
	std::vector<MATERIAL> materials;
	materials = mesh.GetMaterials();

	// マテリアル数分ループしてマテリアルデータを生成
	for (int i = 0; i < materials.size(); i++)
	{
		// マテリアルオブジェクト生成
		std::unique_ptr<CMaterial> m = std::make_unique<CMaterial>();

		// マテリアル情報をセット
		m->Create(materials[i]);

		// マテリアルオブジェクトを配列に追加
		m_Materiales.push_back(std::move(m));
	}
}

void CStaticMeshRenderer::Draw()
{
	// インデックスバッファ・頂点バッファをセット
	BeforeDraw();

	// マテリアル数分ループ 
	for (int i = 0; i < m_Subsets.size(); i++)
	{
		// マテリアルをセット(サブセット情報の中にあるマテリアルインデックを使用する)
		m_Materiales[m_Subsets[i].MaterialIdx]->SetGPU();

		if (m_Materiales[m_Subsets[i].MaterialIdx]->isDiffuseTextureEnable())
		{
			m_DiffuseTextures[m_Subsets[i].MaterialIdx]->SetGPU();
		}

		// サブセットの描画
		DrawSubset(
			m_Subsets[i].IndexNum,							// 描画するインデックス数
			m_Subsets[i].IndexBase,							// 最初のインデックスバッファの位置	
			m_Subsets[i].VertexBase);						// 頂点バッファの最初から使用
	}
}

// 指定されたカラー値をマテリアルに反映させて描画する
// 透明度を指定して描画
void CStaticMeshRenderer::Draw(Color diffusecolor)
{
	// インデックスバッファ・頂点バッファをセット
	BeforeDraw();

	MATERIAL mtrl;
	CMaterial cmtrl{};
	cmtrl.Create(mtrl);

	// マテリアル数分ループ 
	for (int i = 0; i < m_Subsets.size(); i++)
	{
		// 現在のマテリアル情報を取得
		MATERIAL mtrl = m_Materiales[m_Subsets[i].MaterialIdx]->GetMaterial();
		mtrl.Diffuse *= diffusecolor;

		cmtrl.SetMaterial(mtrl);

		cmtrl.SetGPU();

		if (m_Materiales[m_Subsets[i].MaterialIdx]->isDiffuseTextureEnable())
		{
			m_DiffuseTextures[m_Subsets[i].MaterialIdx]->SetGPU();
		}

		// サブセットの描画
		DrawSubset(
			m_Subsets[i].IndexNum,							// 描画するインデックス数
			m_Subsets[i].IndexBase,							// 最初のインデックスバッファの位置	
			m_Subsets[i].VertexBase);						// 頂点バッファの最初から使用
	}
}
