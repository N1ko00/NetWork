#pragma once

#include "room_store.h"

#include <winsock2.h>
#include <chrono>
#include <cstring>
#include <string>

class MatchingServer {
public:
	MatchingServer(uint16_t listenPort, std::chrono::seconds roomTtl);
	bool Run();  // サーバーを起動し、リクエストを処理する

private:
	struct HttpRequest {  // HTTPリクエストを表す構造体
		std::string method;
		std::string target;
		std::string path;
		std::string query;
		std::string body;
	};

	struct HttpResponse {  // HTTPレスポンスを表す構造体
		int statusCode = 200;
		std::string contentType = "application/json; charset=utf-8";
		std::string body;
	};

	bool InitSocket();  // ソケットを初期化する
	void CloseSocket();  // ソケットを閉じる
	bool HandleOneClient();  // クライアントからのリクエストを処理する

	static bool ReadHttpRequest(int sock, HttpRequest& outreq);  // クライアントからHTTPリクエストを読み取る
	static HttpRequest ParseRequestHeadAndBody(const std::string& rawHead, const std::string& body);  // HTTPリクエストのヘッダとボディを解析する
	static std::string BuildHttpResponse(const HttpResponse& res);  // HttpResponse構造体からHTTPレスポンスの文字列を生成する

	HttpResponse RouteRequest(const HttpRequest& req, const std::string& remoteIp);  // HTTPリクエストをルーティングして処理する

	static bool TryGetJsonIntField(const std::string& json, const std::string& key, int& outValue);  // JSON文字列から整数フィールドを取得する
	static bool TryGetJsonStringField(const std::string& json, const std::string& key, std::string& outValue);  // JSON文字列から文字列フィールドを取得する
	static std::string GetQueryParam(const std::string& query, const std::string& key);  // クエリ文字列から指定したキーの値を取得する
	static std::string StatusText(int statusCode);  // HTTPステータスコードからステータステキストを取得する

	uint16_t m_listenPort;
	SOCKET m_listenSock = INVALID_SOCKET;
	RoomStore m_store;
};