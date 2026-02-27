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
#include "../system/CTexture.h"
#include "../system/CVertexBuffer.h"
#include "../system/CMaterial.h"
#include "../system/scenemanager.h"

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

    // カメラを自機に追従
    // 位置は自機の少し上、後方
    if (m_player && m_camera)
    {
        const SRT & playerSrt = m_player->getSRT();
        const Vector3 playerPos = playerSrt.pos;
        const float yaw = playerSrt.rot.y;
        Vector3 forward(-std::sin(yaw), 0.0f, -std::cos(yaw));
        forward.Normalize();
        
        const Vector3 up(0.0f, 1.0f, 0.0f);
        const float cameraHeight = 18.0f;
        const float cameraBackDistance = 28.0f;
        const float lookAheadDistance = 35.0f;
        const float lookUpOffset = 6.0f;

        m_camera->SetPosition(playerPos + up * cameraHeight - forward * cameraBackDistance);
        m_camera->SetLookat(playerPos + up * lookUpOffset + forward * lookAheadDistance);
        m_camera->SetUP(Vector3(0.0f, 1.0f, 0.0f));
    }

    // 受信しているメッセージをすべて処理
    p2pnetworkupdate();
    HandleBulletEnemyCollisions();
    if (HandleEnemyBulletPlayerCollision()) {
        SceneManager::SetCurrentScene("ResultScene");
        return;
    }

    UpdateExplosionEffects(static_cast<float>(deltatime) / 16.6667f);
}

/**
 * @brief 描画処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */
void P2PScene::draw(uint64_t deltatime)
{
	m_camera->Draw();

    if (m_skydomeShader && m_skydomeRenderer && m_camera) {
        SRT skySrt{};
        skySrt.pos = m_camera->GetPosition();
        skySrt.scale = Vector3(1.0f, 1.0f, 1.0f);
        skySrt.rot = Vector3(0.0f, 0.0f, 0.0f);

        Matrix4x4 skyMtx = skySrt.GetMatrix();
        Renderer::SetWorldMatrix(&skyMtx);
        m_skydomeShader->SetGPU();
        m_skydomeRenderer->Draw();
    }


    m_objectmanager->DrawAll(deltatime);
    DrawExplosionEffects();
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

    InitExplosionEffectResources();

    // p2pnetowrk start
    p2pnetworkstart();

    // オブジェクトマネージャを生成
    m_objectmanager = std::make_unique<ObjectManager>(this);

    // フィールド初期化
    m_field=m_objectmanager->CreateLocal<field>();

	// プレイヤ初期化
    m_player = m_objectmanager->CreateLocal<player>();

    // 後から参加した端末にも自分の状態が渡るよう初期同期を送る
    SendRegist();

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

     loadStaticMesh("assets/model/skydome.x", "skydome.x", "assets/model/");
     m_skydomeMesh = MeshManager::getMesh<CStaticMesh>("skydome.x");
     m_skydomeRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("skydome.x");
     m_skydomeShader = MeshManager::getShader<CShader>("unlightshader");

}

/**
 * @brief p2pネットワー初期処理
 */
void P2PScene::p2pnetworkstart()
{
    auto registerHandlers = [this](NetworkSystem& net)
    {
        // 受信メッセージ処理を登録
        net.RegisterHandler(
            MessageType::POSITIONINFO,
            [this](std::unique_ptr<MsgData> msg, uint32_t ip, uint16_t port)
            {
                PositionInfoHandler(std::move(msg), ip, port);
            });

        net.RegisterHandler(
            MessageType::BULLETREGIST,
            [this](std::unique_ptr<MsgData> msg, uint32_t ip, uint16_t port)
            {
                BulletRegistHandler(std::move(msg), ip, port);
            });

        net.RegisterHandler(
            MessageType::REGIST,
            [this](std::unique_ptr<MsgData> msg, uint32_t ip, uint16_t port)
            {
                RegistHandler(std::move(msg), ip, port);
            });

        net.RegisterHandler(
            MessageType::EXPLOSIONSTART,
            [this](std::unique_ptr<MsgData> msg, uint32_t ip, uint16_t port)
            {
                ExplosionStartHandler(std::move(msg), ip, port);
            });
    };

    const std::array<std::string,3> filename={
        "pia1/config.toml",
        "pia2/config.toml",
        "pia3/config.toml"
    };

    struct PeerConfig {
        std::string ip;
        uint16_t port;
    };

    for (std::size_t selectno = 0; selectno < filename.size(); ++selectno) {
        // TOML 読み込み（例外対策）
        toml::value config;
        try {
            config = toml::parse(filename[selectno]);
        }
        catch (const std::exception& e) {
            std::cerr << "toml parse error(" << filename[selectno] << "): " << e.what() << "\n";
            continue;
        }

        int myport_i = 0;
        int machineid_i = 0;
        std::vector<PeerConfig> peers;

        try {
            myport_i = toml::find<int>(config, "myport");
            machineid_i = toml::find<int>(config, "machineID");

            // [[peers]] ... の配列を取得（要素は table）
            const auto peerValues = toml::find<std::vector<toml::value>>(config, "peers");

            if (peerValues.empty()) {
                std::cerr << "toml key error(" << filename[selectno] << "): peers is empty\n";
                continue;
            }
            if (peerValues.size() > 10) {
                std::cerr << "toml key error(" << filename[selectno] << "): peers size over 10: " << peerValues.size() << "\n";
                continue;
            }

            peers.reserve(peerValues.size());
            for (std::size_t i = 0; i < peerValues.size(); ++i) {
                const auto& peer = peerValues[i];

                // peer は table なので、そこから ip/port を読む
                const std::string ip = toml::find<std::string>(peer, "ip");
                const int port_i = toml::find<int>(peer, "port");

                if (ip.empty()) {
                    std::cerr << "toml key error(" << filename[selectno] << "): peers[" << i << "].ip is empty\n";
                    peers.clear();
                    break;
                }

                const auto port_u16 = ToPortU16(port_i);
                if (!port_u16) {
                    std::cerr << "port out of range(" << filename[selectno] << "): peers[" << i << "].port=" << port_i << "\n";
                    peers.clear();
                    break;
                }

                peers.push_back(PeerConfig{ ip, *port_u16 });
            }
        }
        catch (const std::exception& e) {
            std::cerr << "toml key error(" << filename[selectno] << "): " << e.what() << "\n";
            continue;
        }

        if (peers.empty()) {
            continue;
        }

        // myport 範囲チェック
        const auto myport = ToPortU16(myport_i);
        if (!myport) {
            std::cerr << "port out of range(" << filename[selectno] << "). myport=" << myport_i << "\n";
            continue;
        }

        auto trialNet = std::make_unique<NetworkSystem>();
        registerHandlers(*trialNet);

        // 通信相手の登録
        std::string err;
        for (const auto& p : peers) {
            trialNet->AddPeer(p.ip.c_str(), p.port, &err);
        }

        // ネットワーク開始（失敗チェック）
        const bool ok = trialNet->Start(
            *myport,
            peers[0].ip.c_str(),
            peers[0].port,
            [](const std::string& e) {
                if (!e.empty()) std::cerr << "[NetError] " << e << "\n";
            }
        );

        if (!ok) {
            std::cerr << "NetworkSystem::Start failed for " << filename[selectno] << "\n";
          
            continue;
        }

        m_net = std::move(trialNet);

        std::cout << "auto selected config: " << filename[selectno] << "\n";
        std::cout << "myport:" << *myport << "\n";
        std::cout << "machineID:" << machineid_i << "\n";

        m_machineID = static_cast<uint64_t>(machineid_i);

        for (std::size_t i = 0; i < peers.size(); ++i) {
            std::cout << "peers[" << i << "].ip:" << peers[i].ip
                << " port:" << peers[i].port << "\n";
        }

        return;
    }

    std::cerr << "No available pia config found (pia1 -> pia3 all unavailable).\n";
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

void P2PScene::SendRegist()
{
    if (!m_net || !m_player) {
        return;
    }

    const SRT srt = m_player->getSRT();

    MsgData msg{};
    msg.Msg.Header.type = MessageType::REGIST;
    msg.Msg.Header.ID = m_player->GetObjectId();
    msg.Msg.Header.seqenceno = 0;
    msg.Msg.registbody.pos = srt.pos;
    msg.Msg.registbody.rotation = srt.rot;
    msg.Msg.registbody.scale = srt.scale;

    m_net->SendAll(msg);
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
        if (!bullets[bulletIndex].isLocal) {
            continue;
        }
        const Vector3 bulletPos = bullets[bulletIndex].pos;
        bool hit = false;
        ObjectId hitTargetId = 0;

        m_objectmanager->ForEach<enemy>([&](enemy& target)
            {
                if (hit || !target.IsAlive()) {
                    return;
                }

                if (target.CheckHitByBullet(bulletPos)) {
                    hitTargetId = target.GetObjectId();
                    SpawnExplosionEffect(target.getSRT().pos + Vector3(0.0f, m_explosionYOffset, 0.0f), true, hitTargetId);
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

bool P2PScene::HandleEnemyBulletPlayerCollision()
{
    auto& bullets = m_player->GetBullets();
    std::vector<std::size_t> removeIndices;

    for (std::size_t bulletIndex = 0; bulletIndex < bullets.size(); ++bulletIndex)
    {
        if (bullets[bulletIndex].isLocal) {
            continue;
        }

        if (m_player->CheckHitByEnemyBullet(bullets[bulletIndex].pos)) {
            removeIndices.push_back(bulletIndex);
        }
    }

    for (auto it = removeIndices.rbegin(); it != removeIndices.rend(); ++it)
    {
        m_player->RemoveBulletAt(*it);
    }

    return !removeIndices.empty();
}

void P2PScene::InitExplosionEffectResources()
{
    m_explosionShader = MeshManager::getShader<CShader>("unlightshader");

    MATERIAL mtrl{};
    mtrl.Ambient = Color(0, 0, 0, 0);
    mtrl.Diffuse = Color(1, 1, 1, 1);
    mtrl.Emission = Color(0, 0, 0, 0);
    mtrl.Specular = Color(0, 0, 0, 0);
    mtrl.Shiness = 0;
    mtrl.TextureEnable = TRUE;
    m_explosionMaterial.Create(mtrl);

    bool loaded = m_explosionTexture.Load(std::filesystem::path("assets/texture/Eliminate.png"));
    if (!loaded) {
        loaded = m_explosionTexture.Load(std::filesystem::path("assets/texture/Eliminate.png"));
    }
    assert(loaded == true);

    m_explosionVertices.clear();
    m_explosionVertices.resize(4);
    m_explosionVertices[0].Position = Vector3(-0.5f, 0.5f, 0.0f);
    m_explosionVertices[1].Position = Vector3(0.5f, 0.5f, 0.0f);
    m_explosionVertices[2].Position = Vector3(-0.5f, -0.5f, 0.0f);
    m_explosionVertices[3].Position = Vector3(0.5f, -0.5f, 0.0f);

    m_explosionVertices[0].TexCoord = Vector2(0.0f, 0.0f);
    m_explosionVertices[1].TexCoord = Vector2(1.0f, 0.0f);
    m_explosionVertices[2].TexCoord = Vector2(0.0f, 1.0f);
    m_explosionVertices[3].TexCoord = Vector2(1.0f, 1.0f);
    for (auto& v : m_explosionVertices) {
        v.Diffuse = Color(1, 1, 1, 1);
        v.Normal = Vector3(0, 0, -1);
    }
    m_explosionVertexBuffer.Create(m_explosionVertices);
}

void P2PScene::SpawnExplosionEffect(const Vector3& worldPos, bool broadcast, ObjectId hitTargetId)
{
    m_explosionEffects.push_back({ worldPos, m_explosionLifeFrame, m_explosionLifeFrame });

    if (!broadcast || !m_net) {
        return;
    }

    MsgData msg{};
    msg.Msg.Header.type = MessageType::EXPLOSIONSTART;
    msg.Msg.Header.ID = hitTargetId;
    msg.Msg.Header.seqenceno = 0;
    msg.Msg.explosionstartbody.pos = worldPos;
    m_net->SendAll(msg);
}

void P2PScene::UpdateExplosionEffects(float dt)
{
    for (auto& e : m_explosionEffects) {
        e.life -= dt;
    }

    m_explosionEffects.erase(
        std::remove_if(m_explosionEffects.begin(), m_explosionEffects.end(),
            [](const ExplosionEffect& e) { return e.life <= 0.0f; }),
        m_explosionEffects.end());
}

void P2PScene::DrawExplosionEffects()
{
    if (m_explosionEffects.empty() || !m_explosionShader || !m_camera) {
        return;
    }

    Matrix4x4 viewmtx = m_camera->GetViewMatrix();
    Matrix4x4 t = viewmtx.Transpose();

    ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
    devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    m_explosionShader->SetGPU();
    m_explosionVertexBuffer.SetGPU();
    m_explosionMaterial.SetGPU();
    m_explosionTexture.SetGPU();

    for (const auto& e : m_explosionEffects)
    {
        const float lifeRate = std::max(0.0f, e.life / e.maxLife);
        const float size = m_explosionSize * (1.0f + (1.0f - lifeRate) * 0.6f);

        Matrix4x4 world = Matrix4x4::Identity;
        world._11 = t._11 * size; world._12 = t._12 * size; world._13 = t._13 * size;
        world._21 = t._21 * size; world._22 = t._22 * size; world._23 = t._23 * size;
        world._31 = t._31 * size; world._32 = t._32 * size; world._33 = t._33 * size;
        world._41 = e.pos.x;      world._42 = e.pos.y;      world._43 = e.pos.z;

        Renderer::SetWorldMatrix(&world);
        devicecontext->Draw(4, 0);
    }
}

void P2PScene::ExplosionStartHandler(
    std::unique_ptr<MsgData> msg,
    uint32_t ipadr,
    uint16_t port)
{
    (void)ipadr;
    (void)port;

    if (m_player && msg->Msg.Header.ID == m_player->GetObjectId()) {
        return;
    }

    const Vector3 pos = msg->Msg.explosionstartbody.pos;
    SpawnExplosionEffect(pos, false, msg->Msg.Header.ID);
}

void P2PScene::RegistHandler(
    std::unique_ptr<MsgData> msg,
    uint32_t ipadr,
    uint16_t port)
{
    (void)ipadr;
    (void)port;

    SRT srt{};
    srt.pos = msg->Msg.registbody.pos;
    srt.rot = msg->Msg.registbody.rotation;
    srt.scale = msg->Msg.registbody.scale;

    auto obj = m_objectmanager->FindById(msg->Msg.Header.ID);
    const bool isNewRemote = (obj == nullptr);
    if (isNewRemote) {
        obj = m_objectmanager->CreateRemoteWithId<enemy>(msg->Msg.Header.ID);
    }

    if (obj == nullptr) {
        return;
    }

    obj->setSRT(srt);

    if (isNewRemote) {
        SendRegist();
    }
}

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

    if (obj == nullptr) {
        return;
    }

    obj->setSRT(srt);
}
