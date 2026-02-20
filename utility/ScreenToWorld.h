#pragma once

#include	"../system/commontypes.h"

class ScreenToWorld {
public:
	ScreenToWorld()=delete;
	ScreenToWorld(int mousex, int mousey)
		:m_mouseposx(mousex), m_mouseposy(mousey) {
	}
	virtual ~ScreenToWorld() {};

	// 正規デバイス座標系での座標を取得	
	Vector3 GetNDC();

	// ビュー座標系での座標を取得
	Vector3 GetViewCoordinate(
		float depth,
		const Matrix4x4& projmtx);

	// ビュー座標系での座標を取得
	Vector3 GetWorldCoordinate(
		float depth,
		const Matrix4x4& projmtx,
		const Matrix4x4& viewmtx
	);

private:
	int m_mouseposx{};				// マウス座標X	
	int m_mouseposy{};				// マウス座標Y

	float	m_screenwidth{};		// スクリーンの幅
	float	m_screenheight{};		// スクリーンの高さ

	Matrix4x4 m_mtxviewport{};		// ビューポート変換行列
};