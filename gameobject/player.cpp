#include		"player.h"
#include     <algorithm>
#include     <cstddef>
#include     <cmath>
#include    "../system/CDirectInput.h"
#include    "../utility/pathutil.h"
#include		"../system/meshmanager.h"
#include		"../system/camera.h"
#include	"../system/LineDrawer.h"
#include    "../system/SphereDrawer.h"

void player::init() {

	m_mesh = MeshManager::getMesh<CStaticMesh>("tank10_base.x");
	m_shader = MeshManager::getShader<CShader>("lightshader");
	m_meshrenderer = MeshManager::getRenderer<CStaticMeshRenderer>("tank10_base.x");
	MeshManager::getMesh<CStaticMesh>("tank10_top.x");
	MeshManager::getMesh<CStaticMesh>("tank10_cat.x");
	MeshManager::getMesh<CStaticMesh>("tank10_pipe.x");
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_top.x");
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_cat.x");
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_pipe.x");

	m_srt.pos = Vector3(0, 5, 0);
	m_srt.scale = Vector3(1, 1, 1);
	m_srt.rot = Vector3(0, 0, PI);
}

void player::SpawnLocalBullet()
{
	Vector3 shotDir(-std::sin(m_srt.rot.y), 0.0f, -std::cos(m_srt.rot.y));
	shotDir.Normalize();

	Bullet bullet{};
	bullet.pos = m_srt.pos + Vector3(0.0f, 10.0f, 0.0f) + shotDir * 20.0f;
	bullet.vel = shotDir * 15.0f;
	bullet.life = 180.0f;
	bullet.isLocal = true;

	m_bullets.push_back(bullet);
	m_justFiredBullets.push_back(bullet);
}

void player::SpawnNetworkBullet(const Vector3& pos, const Vector3& dir)
{
	Vector3 normalized = dir;
	if (normalized.LengthSquared() <= 1e-6f) return;
	normalized.Normalize();

	Bullet bullet{};
	bullet.pos = pos;
	bullet.vel = normalized * 15.0f;
	bullet.life = 180.0f;
	bullet.isLocal = false;
	m_bullets.push_back(bullet);
}

void player::RemoveBulletAt(std::size_t index)
{
	if (index >= m_bullets.size()) return;
	m_bullets.erase(m_bullets.begin() + static_cast<std::ptrdiff_t>(index));
}

bool player::CheckHitByEnemyBullet(const Vector3& bulletPos) const
{
	const Vector3 center = m_srt.pos + Vector3(0.0f, 10.0f, 0.0f);
	const Vector3 diff = bulletPos - center;
	const float collisionRadius = 18.0f;
	return diff.LengthSquared() <= (collisionRadius * collisionRadius);
}

void player::update(uint64_t dt) {
	// カメラ取得
   IScene* ownerscene = GetOwnerScene();
   const Camera* camera = ownerscene->GetCamera(); 

   const Matrix4x4 vmtx = camera->GetViewMatrix();

   Vector3 forward(vmtx._13, vmtx._23, vmtx._33);	// 進行方向
   Vector3 right(vmtx._11, vmtx._21, vmtx._31);		// 右方向
   Vector3 up(vmtx._12, vmtx._22, vmtx._32);		// 上方向

   // XZ平面上の移動
   forward.y = 0.0f;
   right.y = 0.0f;

   forward.Normalize();
   right.Normalize();
   up.Normalize();

   // 移動方向生成
	// 入力（押してたら +1 / -1）
   const bool keyW = CDirectInput::GetInstance().CheckKeyBuffer(DIK_W);
   const bool keyS = CDirectInput::GetInstance().CheckKeyBuffer(DIK_S);
   const bool keyA = CDirectInput::GetInstance().CheckKeyBuffer(DIK_A);
   const bool keyD = CDirectInput::GetInstance().CheckKeyBuffer(DIK_D);

   float fb = 0.0f; // forward/back
   float lr = 0.0f; // left/right

   if (keyW) fb += 1.0f;
   if (keyS) fb -= 1.0f;
   if (keyD) lr += 1.0f;
   if (keyA) lr -= 1.0f;

   // WASD 移動（カメラ基準）
   Vector3 moveDir = forward * fb + right * lr;
   if (moveDir.LengthSquared() > 1e-6f)
   {
	   moveDir.Normalize(); // 斜めでも速度一定にするならON

	   // 移動量（加算式は元コードに合わせる）
	   m_move += moveDir * VALUE_MOVE_MODEL;

	   float radian = std::atan2(-moveDir.x, -moveDir.z);

	   // [-PI, PI] に正規化（必要なら）
	   if (radian > PI)  radian -= PI * 2.0f;
	   if (radian < -PI) radian += PI * 2.0f;

	   m_destrot.y = radian;
   }

   /// 位置移動
   m_srt.pos += m_move;

   // 移動量に慣性をかける(減速率)
   m_move += -m_move * RATE_MOVE_MODEL;

   // 目標角度と現在角度との差分を求める
   float diffrot = m_destrot.y - m_srt.rot.y;
   if (diffrot > PI)
   {
	   diffrot -= PI * 2.0f;
   }
   if (diffrot < -PI)
   {
	   diffrot += PI * 2.0f;
   }

   // 比率計算
   m_srt.rot.y += diffrot * RATE_ROTATE_MODEL;
   if (m_srt.rot.y > PI)
   {
	   m_srt.rot.y -= PI * 2.0f;
   }
   if (m_srt.rot.y < -PI)
   {
	   m_srt.rot.y += PI * 2.0f;
   }

   // left click to shoot balls
   if (CDirectInput::GetInstance().GetMouseLButtonTrigger())
   {
	   SpawnLocalBullet();
   }

   for (auto& bullet : m_bullets)
   {
	   bullet.pos += bullet.vel;
	   bullet.life -= 1.0f;
   }

   m_bullets.erase(
	   std::remove_if(m_bullets.begin(), m_bullets.end(),
		   [](const Bullet& bullet) { return bullet.life <= 0.0f; }),
	   m_bullets.end());

   // リセット
   if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_RETURN))
   {// リセット
	   m_srt.pos = Vector3(0.0f, 0.0f, 0.0f);
	   m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
   }
}

void player::draw(uint64_t dt) {

	Matrix4x4 mtx = m_srt.GetMatrix();

	Renderer::SetWorldMatrix(&mtx);

	m_shader->SetGPU();
	m_meshrenderer->Draw();
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_top.x")->Draw();
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_cat.x")->Draw();
	MeshManager::getRenderer<CStaticMeshRenderer>("tank10_pipe.x")->Draw();

	// カメラ取得
	IScene* ownerscene = GetOwnerScene();
	const Camera* camera = ownerscene->GetCamera();

	const Matrix4x4 vmtx = camera->GetViewMatrix();

	Vector3 forward(vmtx._13, vmtx._23, vmtx._33);
	Vector3 right(vmtx._11, vmtx._21, vmtx._31);
	Vector3 up(vmtx._12, vmtx._22, vmtx._32);

	forward.Normalize();
	right.Normalize();
	up.Normalize();

	Vector3 start(m_srt.pos.x, m_srt.pos.y, m_srt.pos.z);
	Color col[3] = {
		Color(1,0,0,1),
		Color(0,1,0,1),
		Color(0,1,1,1)
	};

	Vector3 direction[3] ={right,up,forward};

	SetLineWidth(2);
	for (int loopcnt = 0; loopcnt < 3; loopcnt++) {
		LineDrawerDraw(100, 
			start, 
			direction[loopcnt], 
			col[loopcnt]);
	}

	for (const auto& bullet : m_bullets)
	{
		SphereDrawerDraw(5.0f, Color(1.0f, 0.8f, 0.2f, 1.0f), bullet.pos.x, bullet.pos.y, bullet.pos.z);
	}
}

void player::dispose() {

}
