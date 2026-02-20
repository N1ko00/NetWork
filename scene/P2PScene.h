#pragma once

#include "../network/NetworkSystem.h"

#include <array>
#include <memory>
#include <map>

#include "../system/camera.h"
#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"
#include "../system/DirectWrite.h"
#include "../gameobject/field.h"
#include "../gameobject/player.h"
#include "../gameobject/enemy.h"

#include "../gameobject/objectmanager.h"


/**
 * @brief 3Dモデルシーン
 */
class P2PScene : public IScene {
public:
	/// @brief コピーコンストラクタは使用不可
	P2PScene(const P2PScene&) = delete;

	/// @brief 代入演算子も使用不可
	P2PScene& operator=(const P2PScene&) = delete;

	/**
	 * @brief コンストラクタ
	 */
	explicit P2PScene();

	/**
	 * @brief 毎フレームの更新処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 */
	void update(uint64_t deltatime) override;

	/**
	 * @brief 毎フレームの描画処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 *
	 */
	void draw(uint64_t deltatime) override;

	/**
	 * @brief シーンの初期化処理
	 *
	 */
	void init() override;

	/**
	 * @brief シーンの終了処理
	 *
	 */
	void dispose() override;

	/**
	 * @brief カメラの設定
	 */
	void debugUICamera();

	/**
	 * @brief 敵の情報
	 */
	void debugEnemyinfo();

	// リソースを読み込む
	void resourceLoader();

	// ネットワーク初期化
	void p2pnetworkstart();

	// ネットワーク更新
	void p2pnetworkupdate();

	// ネットワーク後始末
	void p2pnetworkdispose();

	// 敵の位置更新ハンドラ
	void PositionInfoHandler(
		std::unique_ptr<MsgData> msg,
		uint32_t ipadr,
		uint16_t port);

	// マシン番号
	uint64_t getmachineid() {
		return m_machineID;
	}
private:	 	
	/**
	 * @brief このシーンで使用するカメラ
	 */
	std::unique_ptr<Camera> m_camera;

	/**
	* @brief フィールド
	*/
	field* m_field{};

	/**
	* @brief プレイヤ
	*/
	player* m_player{};

	/**
	* @brief 敵
	*/
	enemy* m_enemy{};

	// DirectWrite
	std::unique_ptr<DirectWrite> m_directwrite;

	// フォントデータ
	FontData	m_fontdata;

	// P2Pネットワークシステム
	std::unique_ptr<NetworkSystem>m_net{};

	// カメラを取得
	Camera*  GetCamera() override{
		return m_camera.get();
	}

	// このシーン内のゲームオブジェクト実体をすべて持つマネージャ
	std::unique_ptr<ObjectManager> m_objectmanager{};

	// マシン番号（ユニークなIDをマシンマインつけるために必要）
	uint64_t m_machineID = 0;
};

REGISTER_CLASS(P2PScene)

