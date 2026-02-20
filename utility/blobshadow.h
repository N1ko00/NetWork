#pragma once

#include <vector>          // ★ 追加

#include "../system/CommonTypes.h"
#include "../system/transform.h"

#include "../system/CMesh.h"
#include "../system/CMeshRenderer.h"
#include "../system/CMaterial.h"
#include "../system/CShader.h"
#include "../system/CTexture.h"

/// ------------------------------------------------------------
/// 丸影（ブロブシャドウ）
///   - XZ 平面の板ポリ＋丸影テクスチャ
///   - ブレンドは呼び出し側で「減算ブレンド」にしてから Draw()/DrawBatch() を呼ぶ
/// ------------------------------------------------------------
class BlobShadow
{
public:
    BlobShadow();
    ~BlobShadow();

    struct Face {					// 三角形のインデックスデータ
        int		idx[3];
    };

    /// 初期化
    /// @param texturePath 丸影用テクスチャファイルパス
    /// @return 成功なら true
    bool Init(const char* texturePath);

    /// 終了処理
    void Terminate();

    /// 影の色（減算する量）を設定（単発描画用のデフォルト色）
    /// 例: Color(0.3f, 0.3f, 0.3f, 1.0f)
    void SetShadowColor(const Color& color) { m_ShadowColor = color; }

    /// 地面から少し浮かせるオフセット（Zファイティング防止）
    void SetHeightOffset(float offset) { m_HeightOffset = offset; }

    /// --------------------------------------------------------
    /// 【単発】丸影を1つ描画
    ///   - 内部では m_ShadowColor を使う
    /// --------------------------------------------------------
    /// @param radius   球の半径（影のサイズに利用）
    /// @param groundY  地面のY座標
    /// @param ex, ez   影の XZ 位置（＝球の位置の XZ ）
    void Draw(float radius, float groundY, float ex, float ez);

    /// --------------------------------------------------------
    /// 【大量描画用】丸影インスタンス情報
    /// --------------------------------------------------------
    struct ShadowInstance
    {
        float radius = 1.0f;  ///< 影の半径（元の球半径）
        float groundY = 0.0f; ///< 地面のY座標
        float x = 0.0f;       ///< 影のX座標
        float z = 0.0f;       ///< 影のZ座標
        Color color = Color(0.3f, 0.3f, 0.3f, 1.0f); ///< インスタンス毎の影色
    };

    /// 丸影バッチ用のメモリをあらかじめ確保しておく（任意）
    void ReserveBatch(size_t count) { m_ShadowList.reserve(count); }

    /// 登録済みの丸影インスタンスを全てクリア
    void ClearBatch() { m_ShadowList.clear(); }

    /// 丸影インスタンスを1つ追加（色指定版）
    void AddShadow(
        float radius,
        float groundY,
        float ex,
        float ez,
        const Color& color
    )
    {
        ShadowInstance inst;
        inst.radius = radius;
        inst.groundY = groundY;
        inst.x = ex;
        inst.z = ez;
        inst.color = color;
        m_ShadowList.push_back(inst);
    }

    /// 丸影インスタンスを1つ追加（デフォルト色版）
    void AddShadow(
        float radius,
        float groundY,
        float ex,
        float ez
    )
    {
        AddShadow(radius, groundY, ex, ez, m_ShadowColor);
    }

    /// 登録済みの丸影インスタンスをすべて描画
    ///  ※ 事前にブレンドモードを「減算ブレンド」にしておくこと
    void DrawBatch();

private:
    BlobShadow(const BlobShadow&) = delete;
    BlobShadow& operator=(const BlobShadow&) = delete;

    /// 単一インスタンス描画の共通処理
    void DrawOne(float radius, float groundY, float ex, float ez, const Color& color);

private:
    float m_width = 1.0f;		// 幅
    float m_depth = 1.0f;		// 奥行

    int	m_divX = 1;				// X方向分割数
    int	m_divZ = 1;				// Z方向分割数

    // 描画の為の情報（メッシュに関わる情報）
    CIndexBuffer				m_IndexBuffer;				// インデックスバッファ
    CVertexBuffer<VERTEX_3D>	m_VertexBuffer;				// 頂点バッファ

    // 描画の為の情報（見た目に関わる部分）
    CShader						m_Shader;					// シェーダー
    CMaterial					m_Material;					// マテリアル
    CTexture					m_Texture;					// テクスチャ

    std::vector<VERTEX_3D>		m_vertices;					// 頂点群
    std::vector<uint32_t>		m_indices;					// インデックス群

    Color m_ShadowColor = Color(0.3f, 0.3f, 0.3f, 1.0f);
    float m_HeightOffset = 0.01f;    // 地面から少しだけ浮かせる

    /// 大量の丸影を扱うためのバッファ
    std::vector<ShadowInstance> m_ShadowList;

    // インデックスバッファ生成
    void CreateIndexCW();

    // XZ頂点バッファ生成
    void CreateVertexXZ();
};