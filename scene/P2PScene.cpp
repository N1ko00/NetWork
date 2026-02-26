#include <iostream>
#include <algorithm>
#include "P2PScene.h"
#include "../system/DebugUI.h"
#include "../system/meshmanager.h"
#include "../network/toml.hpp"
#include "../utility/pathutil.h"
#include "../system/CStaticMesh.h"
#include "../system/CStaticMeshRenderer.h"
#include "../system/LineDrawer.h"
#include "../system/SphereDrawer.h"

namespace {
    //------------------------------------------------------------------------------
    // port範囲チェック付き変換
    //------------------------------------------------------------------------------
    static std::optional<uint16_t> ToPortU16(int p)
    {
        if (p < 0 || p > 65535) return std::nullopt;
        return static_cast<uint16_t>(p);
    }
}

/**
 * @brief コンストラクタ
 */
P2PScene::P2PScene()
{
}

/**
 * @brief カメラの設定
 *
 */
void P2PScene::debugUICamera() {

	static float azimuth = 0.0f;
	static float elevation = 0.0f;
	static float radius = 1000.0f;

	ImGui::Begin("debug Camera");

	Vector3 camerapos = m_camera->GetPosition();
	Vector3 lookatpos = m_camera->GetLookat();
	Vector3 up = m_camera->GetUP();

	ImGui::SliderFloat3("camera pos ", &camerapos.x, -PI, PI);
	ImGui::SliderFloat3("camera up  ", &up.x, -PI, PI);

	ImGui::Separator();
	ImGui::Separator();

	ImGui::SliderFloat("azimuth ", &azimuth,-PI,PI);
	ImGui::SliderFloat("elevation ", &elevation, -PI/2.0f, PI / 2.0f);
	ImGui::SliderFloat("radius ", &radius, 1.0f, 10000.0f);

	CPolor3D polor(radius, elevation,azimuth);
	Vector3 cpos = polor.ToCartesian();

	CPolor3D polorup(1.0f, elevation+PI/2.0f,azimuth);
	Vector3 upvector = polorup.ToCartesian();;

    m_camera->SetPosition(cpos + lookatpos);
    m_camera->SetLookat(lookatpos);
    m_camera->SetUP(upvector);

    Matrix4x4 vmtx = m_camera->GetViewMatrix();

    Vector3 cameraforward(vmtx._13, vmtx._23, vmtx._33);
    Vector3 camerabackward(-vmtx._13, -vmtx._23, -vmtx._33);
    Vector3 cameraleft(vmtx._11, vmtx._21, vmtx._31);
    Vector3 cameraright(-vmtx._11, -vmtx._21, -vmtx._31);

    cameraforward.Normalize();
    camerabackward.Normalize();
    cameraleft.Normalize();
    cameraright.Normalize();

    ImGui::SliderFloat3("camera forward ", &cameraforward.x, -1, 1);
    ImGui::SliderFloat3("camera backward ", &camerabackward.x, -1, 1);
    ImGui::SliderFloat3("camera left ", &cameraright.x, -1, 1);
    ImGui::SliderFloat3("camera right ", &cameraright.x, -1, 1);

	ImGui::End();
}

/**
 * @brief 敵の情報
 *
 */
void P2PScene::debugEnemyinfo() {

    static float azimuth = 0.0f;
    static float elevation = 0.0f;
    static float radius = 1000.0f;

    ImGui::Begin("debug Enemyinfo");

    SRT srt = m_enemy->getSRT();

    ImGui::SliderFloat3("enemy pos ", &srt.pos.x, -1000, 1000);
    ImGui::SliderFloat3("enemy rot ", &srt.rot.x, -PI, PI);
    ImGui::SliderFloat3("enemy scale ", &srt.scale.x, -10, 10);

    ImGui::End();
}

/**
 * @brief シーンの更新処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */
void P2PScene::update(uint64_t deltatime)
{
    m_objectmanager->UpdateAll(deltatime);

    // 受信しているメッセージをすべて処理
    p2pnetworkupdate();
    HandleBulletEnemyCollisions();
}

/**
 * @brief 描画処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */
void P2PScene::draw(uint64_t deltatime)
{
	m_camera->Draw();

    m_objectmanager->DrawAll(deltatime);
}

/**
 * @brief シーンの初期化処理
 */
void P2PScene::init()
{
	// カメラ(3D)の初期化
	m_camera = std::make_unique <Camera>(Vector3(0,0,-100),Vector3(0,0,0),Vector3(0,1,0));

	// DirectWrite生成
	m_directwrite = std::make_unique<DirectWrite>(&m_fontdata);
	m_directwrite->Init(Renderer::GetSwapChain());

	// リソースを読み込む
	resourceLoader();

    // p2pnetowrk start
    p2pnetworkstart();

    // オブジェクトマネージャを生成
    m_objectmanager = std::make_unique<ObjectManager>(this);

    // フィールド初期化
    m_field=m_objectmanager->CreateLocal<field>();

	// プレイヤ初期化
    m_player = m_objectmanager->CreateLocal<player>();


    // カメラの設定
	DebugUI::RedistDebugFunction([this]() {
		debugUICamera();
		});

    // 線描初期化
    LineDrawerInit();
    SphereDrawerInit();

}

/**
 * @brief シーンの終了処理
 */
void P2PScene::dispose()
{
}

/**
 * @brief リソースを読み込む
 */
void P2PScene::resourceLoader()
{
	// 光源計算なしシェーダー
	std::unique_ptr<CShader> shader1 = std::make_unique<CShader>();
	shader1->Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");
	MeshManager::RegisterShader<CShader>("lightshader", std::move(shader1));

	std::unique_ptr<CShader> shader2 = std::make_unique<CShader>();
	shader2->Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");
	MeshManager::RegisterShader<CShader>("unlightshader", std::move(shader2));

    auto loadStaticMesh = [](const char* assetPath, const char* registerKey, const char* dirPath)
    {
        std::filesystem::path  fpath = utility::PathFromUtf8OrCp932(assetPath);
        std::filesystem::path  dirpath = utility::PathFromUtf8OrCp932(dirPath);

        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load(fpath, dirpath);

        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);

        MeshManager::RegisterMesh<CStaticMesh>(registerKey, std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(registerKey, std::move(renderer));
    };

    loadStaticMesh("assets/model/tank/tank10_base.x", "tank10_base.x", "assets/model/tank/");
    loadStaticMesh("assets/model/tank/tank10_top.x", "tank10_top.x", "assets/model/tank/");
    loadStaticMesh("assets/model/tank/tank10_cat.x", "tank10_cat.x", "assets/model/tank/");
    loadStaticMesh("assets/model/tank/tank10_pipe.x", "tank10_pipe.x", "assets/model/tank/");

     {
        std::filesystem::path  fpath = utility::PathFromUtf8OrCp932("assets/model/car001.x");
        std::filesystem::path  dirpath = utility::PathFromUtf8OrCp932("assets/model/");

        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load(fpath, dirpath);
        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);

        MeshManager::RegisterMesh<CStaticMesh>("car001.x", std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("car001.x", std::move(renderer));
    }

}

/**
 * @brief p2pネットワー初期処理
 */
void P2PScene::p2pnetworkstart()
{
    // NetworkSystem の生成
    m_net = std::make_unique<NetworkSystem>();

    // 受信メッセージ処理を登録
    m_net->RegisterHandler(
        MessageType::POSITIONINFO,
        [this](std::unique_ptr<MsgData> msg, uint32_t ip, uint16_t port)
        {
            PositionInfoHandler(std::move(msg), ip, port);
        });

    m_net->RegisterHandler(
        MessageType::BULLETREGIST,
        [this](std::unique_ptr<MsgData> msg, uint32_t ip, uint16_t port)
        {
            BulletRegistHandler(std::move(msg), ip, port);
        });

    // 設定ファイル選択
    std::cout << "Select pia1 or pia2 or pia3\n";
    std::cout << " 0) pia1\n";
    std::cout << " 1) pia2\n";
    std::cout << " 2) pia3\n";

    int selectno = 0;
    if (!(std::cin >> selectno) || (selectno != 0 && selectno != 1 && selectno != 2)) {
        std::cerr << "invalid selection.\n";
        return;
    }

    const std::string filename[3] = {
        "pia1/config.toml",
        "pia2/config.toml",
        "pia3/config.toml"
    };

    // TOML 読み込み（例外対策）
    toml::value config;
    try {
         config = toml::parse(filename[selectno]);
    }
    catch (const std::exception& e) {
        std::cerr << "toml parse error: " << e.what() << "\n";
        return;
    }

    int myport_i = 0;
    int machineid_i = 0;

     // peers は複数になる
     struct PeerConfig {
         std::string ip;
         uint16_t port;
     };
     std::vector<PeerConfig> peers;

     try {
         myport_i = toml::find<int>(config, "myport");
         machineid_i = toml::find<int>(config, "machineID");

         // [[peers]] ... の配列を取得（要素は table）
         const auto peerValues = toml::find<std::vector<toml::value>>(config, "peers");

         if (peerValues.empty()) {
             std::cerr << "toml key error: peers is empty\n";
             return;
         }
         if (peerValues.size() > 10) {
             std::cerr << "toml key error: peers size over 10: " << peerValues.size() << "\n";
             return;
         }

         peers.reserve(peerValues.size());
         for (std::size_t i = 0; i < peerValues.size(); ++i) {
             const auto& peer = peerValues[i];

             // peer は table なので、そこから ip/port を読む
             const std::string ip = toml::find<std::string>(peer, "ip");
             const int port_i = toml::find<int>(peer, "port");

             if (ip.empty()) {
                    std::cerr << "toml key error: peers[" << i << "].ip is empty\n";
                    return;
             }

             const auto port_u16 = ToPortU16(port_i);
             if (!port_u16) {
                 std::cerr << "port out of range: peers[" << i << "].port=" << port_i << "\n";
                 return;
             } 

             peers.push_back(PeerConfig{ ip, *port_u16 });
         }
     }

     catch (const std::exception& e) {

            std::cerr << "toml key error: " << e.what() << "\n";
            return;
     }

     // 通信相手の登録
     std::string err;
     for (const auto& p : peers) {
         m_net->AddPeer(p.ip.c_str(), p.port,&err);
     }

     // myport 範囲チェック
     const auto myport = ToPortU16(myport_i);
     if (!myport) {
        std::cerr << "port out of range. myport=" << myport_i << "\n";
        return;
     }

     // 表示
     std::cout << "myport:" << *myport << "\n";
     std::cout << "machineID:" << machineid_i << "\n";

     m_machineID = static_cast<uint64_t>(machineid_i);

     for (std::size_t i = 0; i < peers.size(); ++i) {
         std::cout << "peers[" << i << "].ip:" << peers[i].ip
             << " port:" << peers[i].port << "\n";
     }

    // ネットワーク開始（失敗チェック）
    const bool ok = m_net->Start(
        *myport,
        peers[0].ip.c_str(),
        peers[0].port,
        [](const std::string& e) {
            // ここでUI/ログ（最低限 stderr に出す）
            if (!e.empty()) std::cerr << "[NetError] " << e << "\n";
        }
    );

    if (!ok) {
        std::cerr << "NetworkSystem::Start failed.\n";
        return;
    }
}
/**
 * @brief p2pネットワー更新処理
 */
void P2PScene::p2pnetworkupdate()
{
    m_net->PumpIncoming();      // 受信データ処理

    SRT srt;
    srt = m_player->getSRT();

    MsgData msg{};
    msg.Msg.Header.type = MessageType::POSITIONINFO;
    msg.Msg.Header.ID = m_player->GetObjectId();
    msg.Msg.Header.seqenceno = 0;
    msg.Msg.posinfobody.pos = srt.pos;
    msg.Msg.posinfobody.rotation = srt.rot;
    msg.Msg.posinfobody.scale = srt.scale;

    m_net->SendAll(msg);

    SendBulletRegist();
    m_player->ClearJustFiredBullets();
}

void P2PScene::SendBulletRegist()
{
    const auto& fired = m_player->GetJustFiredBullets();
    for (const auto& b : fired)
    {
        MsgData bmsg{};
        bmsg.Msg.Header.type = MessageType::BULLETREGIST;
        bmsg.Msg.Header.ID = m_player->GetObjectId();
        bmsg.Msg.Header.seqenceno = 0;
        bmsg.Msg.bulletregistbody.pos = b.pos;
        bmsg.Msg.bulletregistbody.rotation = b.vel;
        bmsg.Msg.bulletregistbody.scale = Vector3(1.0f, 1.0f, 1.0f);
        m_net->SendAll(bmsg);
    }
}

void P2PScene::HandleBulletEnemyCollisions()
{
    auto& bullets = m_player->GetBullets();
    if (bullets.empty()) {
        return;
    }

    std::vector<std::size_t> hitBulletIndices;
    hitBulletIndices.reserve(bullets.size());

    for (std::size_t bulletIndex = 0; bulletIndex < bullets.size(); ++bulletIndex)
    {
        const Vector3 bulletPos = bullets[bulletIndex].pos;
        bool hit = false;

        m_objectmanager->ForEach<enemy>([&](enemy& target)
            {
                if (hit || !target.IsAlive()) {
                    return;
                }

                if (target.CheckHitByBullet(bulletPos)) {
                    hit = true;
                }
            });

        if (hit) {
            hitBulletIndices.push_back(bulletIndex);
        }
    }

    for (auto it = hitBulletIndices.rbegin(); it != hitBulletIndices.rend(); ++it)
    {
        m_player->RemoveBulletAt(*it);
    }
}

/**
 * @brief eʒmM
 */
void P2PScene::BulletRegistHandler(
    std::unique_ptr<MsgData> msg,
    uint32_t ipadr,
    uint16_t port)
{
    (void)ipadr;
    (void)port;

    const Vector3 pos = msg->Msg.bulletregistbody.pos;
    const Vector3 dir = msg->Msg.bulletregistbody.rotation;
    m_player->SpawnNetworkBullet(pos, dir);
}

/**
 * @brief p2pネットワーク後始末
  */
void P2PScene::p2pnetworkdispose()
{

}

// 敵の位置更新ハンドラ
void P2PScene::PositionInfoHandler(
    std::unique_ptr<MsgData> msg, 
    uint32_t ipadr, 
    uint16_t port)
{

    SRT srt{};
    // 受信した相手座標を保持しておく
    srt.pos.x = msg->Msg.posinfobody.pos.x;
    srt.pos.y = msg->Msg.posinfobody.pos.y;
    srt.pos.z = msg->Msg.posinfobody.pos.z;

    srt.rot.x = msg->Msg.posinfobody.rotation.x;
    srt.rot.y = msg->Msg.posinfobody.rotation.y;
    srt.rot.z = msg->Msg.posinfobody.rotation.z;

    srt.scale.x = msg->Msg.posinfobody.scale.x;
    srt.scale.y = msg->Msg.posinfobody.scale.y;
    srt.scale.z = msg->Msg.posinfobody.scale.z;

    // 該当オブジェクトをサーチ
    auto obj = m_objectmanager->FindById(msg->Msg.Header.ID);
    if (obj == nullptr) {
        // プレイヤ初期化
        obj = m_objectmanager->CreateRemoteWithId<enemy>(msg->Msg.Header.ID);
    }

    obj->setSRT(srt);
}
