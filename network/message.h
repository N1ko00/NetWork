#pragma once
#include	"../system/commontypes.h"

enum class MessageType {
	POSITIONINFO,		// ポジション通知
	REGIST,				// クライアント登録
	ELIMINATE,			// クライアント削除
	BULLETINFO,			// 弾情報
	BULLETREGIST,		// 弾登録
	BULLETELIMINATE,	// 弾削除
	EXPLOSIONSTART		// 爆発開始
};

/*----------------------------------------------
	メッセージ定義
------------------------------------------------*/
// メッセージヘッダー
struct	_Header {
	MessageType	type;
	uint16_t	seqenceno;
	uint64_t    ID;				// object id(snow flake id)
};

// メッセージボディ（メッセージタイプ０）（ポジション通知）
struct	_POSITIONINFOBODY {
	Vector3 pos;				// 位置
	Vector3 rotation;			// 姿勢（角度）
	Vector3 scale;				// スケール
};

// メッセージボディ（メッセージタイプ１）（クライアント登録）
struct	_REGISTBODY {
	Vector3 pos;							// 位置
	Vector3 rotation;						// 方向
	Vector3 scale;							// スケール
};

// メッセージボディ（メッセージタイプ２）（クライアント削除）
struct	_ELIMINATEBODY {
};

// メッセージボディ（メッセージタイプ３）（弾情報）
struct	_BULLETINFOBODY {
	Vector3 pos;							// 位置
	Vector3 rotation;						// 姿勢角度
	Vector3 scale;							// スケール
};

// メッセージボディ（メッセージタイプ４）（弾登録）
struct	_BULLETREGISTBODY {
	Vector3 pos;							// 位置
	Vector3 rotation;						// 方向
	Vector3 scale;							// スケール
};

// メッセージボディ（メッセージタイプ５）（弾削除）
struct	_BULLETELIMINATEBODY {
};

// メッセージボディ（メッセージタイプ６）（爆発開始）
struct	_EXPLOSIONSTARTBODY {
	Vector3 pos;							// 位置
};

// メッセージの構造
struct	_Message {
	_Header	Header;
	union {
		_POSITIONINFOBODY		posinfobody;			// ポジション通知
		_REGISTBODY				registbody;				// クライアント登録
		_ELIMINATEBODY			eliminatebody;			// クライアント削除
		_BULLETINFOBODY			bullteinfobody;			// 弾情報
		_BULLETREGISTBODY		bulletregistbody;		// 弾情報登録
		_BULLETELIMINATEBODY	bullteeliminatebody;	// 弾情報削除
		_EXPLOSIONSTARTBODY		explosionstartbody;		// 爆発開始
	};
};

// 送受信メッセージ構造体
struct	MsgData {
	union {
		_Message  Msg;
		char	data[sizeof(_Message)];
	};
	MsgData() {
		memset(data, 0, sizeof(data));
	}
};