#include "ScreenToWorld.h"
#include "../application.h"

Vector3 ScreenToWorld::GetNDC() {

	// ビューポート変換行列作成
	m_mtxviewport = Matrix4x4::Identity;
	m_mtxviewport._11 =  Application::GetWidth() / 2.0f;
	m_mtxviewport._22 = -1.0f * (Application::GetHeight() / 2.0f);
	m_mtxviewport._41 = Application::GetWidth() / 2.0f;
	m_mtxviewport._42 = Application::GetHeight() / 2.0f;

	// スクリーン座標をVECTOR3に格納
	Vector3 screenPos(
		static_cast<float>(m_mouseposx), 
		static_cast<float>(m_mouseposy), 0.0f);

	Matrix4x4 invvp = m_mtxviewport.Invert();

	// スクリーン座標をワールド座標に変換
	Vector3 NDCPos = screenPos.Transform(screenPos, invvp);

	return NDCPos;
}

// ビュー座標系での座標を取得
Vector3 ScreenToWorld::GetViewCoordinate(
	float depth,
	const Matrix4x4& projmtx) {

	// 正規デバイス座標系での座標を取得
	Vector3 NDCPos = GetNDC();
	NDCPos.z = depth;

	// クリップ空間座標系での座標を取得
	Matrix4x4 invproj = projmtx.Invert();		// プロジェクション変換行列の逆行列を求める

	// 正規デバイス座標をクリップ空間座標に変換
	Vector3 viewPos = NDCPos.Transform(NDCPos, invproj);

	// Wで割り算
	float w = NDCPos.x * invproj._14 +
		NDCPos.y * invproj._24 +
		NDCPos.z * invproj._34 +
		invproj._44;

	viewPos.x /= w;
	viewPos.y /= w;
	viewPos.z /= w;

	return viewPos;
}

// ワールド座標系での座標を取得
Vector3 ScreenToWorld::GetWorldCoordinate(
		float depth,
		const Matrix4x4& projmtx,
		const Matrix4x4& viewmtx
) {

	// ビュー座標系での座標を取得
	Vector3 clippos = GetViewCoordinate(
		depth,
		projmtx);

	// ビュー空間座標系での座標を取得
	Matrix4x4 invview = viewmtx.Invert();		// ビュー変換行列の逆行列を求める

	// クリップ空間座標をビュー空間座標系に変換
	Vector3 viewPos = clippos.Transform(clippos, invview);

	// Wで割り算
	float w = viewPos.x * invview._14 +
		viewPos.y * invview._24 +
		viewPos.z * invview._34 +
		invview._44;

	viewPos.x /= w;
	viewPos.y /= w;
	viewPos.z /= w;

	return viewPos;
}