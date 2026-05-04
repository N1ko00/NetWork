#include "matching_server.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

namespace {
	std::string Trim(const std::string& s) {  // 文字列の前後の空白を削除する
		size_t b = 0;
		while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
		size_t e = s.size();
		while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
		return s.substr(b, e - b);
	}

	// HTTPリクエストのターゲットをパスとクエリに分割する
	bool ParseRequestTarget(const std::string& target, std::string& path, std::string& query) {
		const auto pos = target.find('?');
		if (pos == std::string::npos) {
			path = target;
			query.clear();
			return true;
		}
		path = target.substr(0, pos);
		query = target.substr(pos + 1);
		return true;
	}

	std::string SocketAddToIp(const sockaddr_in& addr) {  // ソケットアドレスからIPアドレスを文字列に変換する
		char buf[INET_ADDRSTRLEN] = {};
		inet_ntop(AF_INET, &(addr.sin_addr), buf, static_cast<socklen_t>(sizeof(buf)));
		return std::string(buf);
	}
}

MatchingServer::MatchingServer(uint16_t listenPort, std::chrono::seconds roomTtl)
	: m_listenPort(listenPort), m_store(roomTtl) {}

bool MatchingServer::Run() {
	if (!InitSocket()) {
		return false;
	}

	std::cout << "[MatchinServer] listen 0.0.0.0:" << m_listenPort << std::endl;
	while (true) {
		if (!HandleOneClient()) {  // クライアントの処理に失敗してもサーバーは継続する
			::Sleep(1);  // クライアント処理に失敗した場合は少し待ってから次のクライアントを処理する
		}
	}
}

bool MatchingServer::InitSocket() {
	WSADATA wsa{};  // Winsockの初期化
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {  // Winsockの初期化に失敗した場合はエラーメッセージを出力してfalseを返す
		std::cerr << "[MatchingServer]WSAStartup failed." << std::endl;
		return false;
	}

	sockaddr_in addr{};  // ソケットアドレス構造体をゼロクリアする
	addr.sin_family = AF_INET;  // IPv4を使用する
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // すべてのインターフェースで接続を受け付けるためにINADDR_ANYを使用する
	addr.sin_port = htons(m_listenPort);  // ポート番号をネットワークバイトオーダーに変換する

	// ソケットを作成する
	if (::bind(static_cast<SOCKET>(m_listenSock), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		std::cerr<<"[MatchingServer] bind Failed."<<std::endl;
		return false;
	}

	// ソケットをリッスン状態にする
	if (::listen(static_cast<SOCKET>(m_listenSock), SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "[MatchingServer] listen Failed." << std::endl;
		return false;
	}
	return true;
}

void MatchingServer::CloseSocket() {
	if (m_listenSock >= 0) {
		closesocket(static_cast<SOCKET>(m_listenSock));  // ソケットを閉じる
		m_listenSock = -1;
	}
	WSACleanup();  // Winsockのクリーンアップ
}

bool MatchingServer::HandleOneClient() {
	sockaddr_in clientAddr{};  // クライアントのソケットアドレス構造体をゼロクリアする
	int len = sizeof(clientAddr);  // クライアントのソケットアドレス構造体のサイズを設定する

	// クライアントからの接続を受け入れる
	SOCKET client = ::accept(static_cast<SOCKET>(m_listenSock), reinterpret_cast<sockaddr*>(&clientAddr), &len);  // クライアントからの接続を受け入れる
	if (client == INVALID_SOCKET) {
		return false;
	}

	// クライアントからHTTPリクエストを読み取る
	HttpRequest req{};
	if (ReadHttpRequest(static_cast<int>(client), req)) {  // クライアントからHTTPリクエストを読み取る
		const HttpResponse bad{  // HTTPリクエストの読み取りに失敗した場合は400 Bad Requestを返す
			400, "application/json; charset=utf-8",
			"{\"error\":{\"code\":\"bad_request\",\"message\":\"invalid http request\"}}"
	    };
		const std::string raw = BuildHttpResponse(bad);
		::send(client, raw.data(), static_cast<int>(raw.size()), 0);
		closesocket(client);
		return true;
	}

	//HTTPリクエスト送信元IPをendpointとして採用
	//クライアント自己申告IPは信頼できないため、ソケットから取得したIPアドレスを使用する
	const std::string remoteIp = SocketAddToIp(clientAddr);

	const HttpResponse res = RouteRequest(req, remoteIp);  // HTTPリクエストをルーティングして処理する
	const std::string raw = BuildHttpResponse(res);  // HttpResponse構造体からHTTPレスポンスの文字列を生成する
	::send(client, raw.data(), static_cast<int>(raw.size()), 0);  // クライアントにHTTPレスポンスを送信する
	closesocket(client);  // クライアントとの接続を閉じる
	return true;
}

bool MatchingServer::ReadHttpRequest(int sock, HttpRequest& outreq) {
	std::string buf;
	buf.reserve(4096);
	
	char tmp[1024];
	int contentLength = 0;
	size_t headerEnd = std::string::npos;
	
	// HTTPリクエストのヘッダとボディを読み取る
	while (true)
	{
		int n = ::recv(static_cast<SOCKET>(sock), tmp, sizeof(tmp), 0);
		if (n <= 0) return false;
		buf.append(tmp, tmp + n);
	
		headerEnd = buf.find("\r\n\r\n");
		if (headerEnd != std::string::npos) break;
		if (buf.size() > 1024 * 1024) return false;
	}
	
	std::string header = buf.substr(0, headerEnd + 4);
	std::string body = buf.substr(headerEnd + 4);
	
	{
		std::istringstream iss(header);  // ヘッダを行単位で読み取るための文字列ストリームを作成する
		std::string line;  // 最初の行はリクエストラインなので読み飛ばす
		std::getline(iss, line);  
		while (std::getline(iss, line))  // ヘッダの行を1行ずつ読み取る
		{
			line = Trim(line);
			if (line.empty()) continue;
			auto p = line.find(':');
			if (p == std::string::npos) continue;
			const std::string key = Trim(line.substr(0, p));
			const std::string val = Trim(line.substr(p + 1));
			if (_stricmp(key.c_str(), "Content-Length") == 0){
				contentLength = std::max(0, atoi(val.c_str()));
			}
		}
	}
	
	// HTTPリクエストのボディを読み取る
	while (static_cast<int>(body.size()) < contentLength)
	{
		int n = ::recv(static_cast<SOCKET>(sock), tmp, sizeof(tmp), 0);
		if (n <= 0) return false;
		body.append(tmp, tmp + n);
	}
	if (contentLength >= 0 && static_cast<int>(body.size()) > contentLength){
		body.resize(contentLength);
	}
	
	// HTTPリクエストのヘッダとボディを解析してHttpRequest構造体に格納する
	outreq = ParseRequestHeadAndBody(header, body);
	return !outreq.method.empty();
}

MatchingServer::HttpRequest MatchingServer::ParseRequestHeadAndBody(const std::string& rawHead, const std::string& body) {
	HttpRequest req{};
	req.body = body;

	std::istringstream iss(rawHead);  // ヘッダを行単位で読み取るための文字列ストリームを作成する
	std::string requestLine;  // 最初の行はリクエストラインなので読み取る
	if (!std::getline(iss, requestLine)) return req;

	requestLine = Trim(requestLine);
	std::istringstream rl(requestLine);  // リクエストラインを空白で分割するための文字列ストリームを作成する
	std::string version;
	rl >> req.method >> req.target >> version;  // リクエストラインからHTTPメソッド、ターゲット、バージョンを読み取る
	if (req.method.empty() || req.target.empty())return HttpRequest{};

	ParseRequestTarget(req.target, req.path, req.query);  // HTTPリクエストのターゲットをパスとクエリに分割する
	return req;
}

MatchingServer::HttpResponse MatchingServer::RouteRequest(const HttpRequest& req, const std::string& remoteIp) {
	if (req.method == "POST" && req.path == "/create") {  
		int hostUdpPort = 0;
		if (!TryGetJsonIntField(req.body, "hostUdpPort", hostUdpPort) || hostUdpPort <= 0 || hostUdpPort > 65535){
			// エラー応答を返す処理:
			// 不正portはP2P開始不能なのでここで即時に弾く。
			return { 400, "application/json; charset=utf-8", "{\"error\":{\"code\":\"invalid_port\",\"message\":\"hostUdpPort must be 1..65535\"}}" };
		}

		Endpoint hostEP{ remoteIp, static_cast<uint16_t>(hostUdpPort) };  // HTTPリクエストのボディからhostUdpPortフィールドを取得する。
		ApiResult r = m_store.CreateRoom(hostEP);  // RoomStoreのCreateRoomメソッドを呼び出してルームを作成
		return { r.statusCode, "application/json; charset=utf-8", r.bodyJson };  // RoomStoreのCreateRoomメソッドの結果をHTTPレスポンスのボディに設定して返す
	}

	if (req.method == "POST" && req.path == "/join"){
		std::string roomId;
		int joinUdpPort = 0;

		// HTTPリクエストのボディからroomIdフィールドとjoinUdpPortフィールドを取得する。
		if (!TryGetJsonStringField(req.body, "roomId", roomId) || roomId.empty()){  
			return { 400, "application/json; charset=utf-8", "{\"error\":{\"code\":\"invalid_room_id\",\"message\":\"roomId is required\"}}" };
		}

		// 不正portはP2P開始不能なのでここで即時に弾く。
		if (!TryGetJsonIntField(req.body, "joinUdpPort", joinUdpPort) || joinUdpPort <= 0 || joinUdpPort > 65535){
			return { 400, "application/json; charset=utf-8", "{\"error\":{\"code\":\"invalid_port\",\"message\":\"joinUdpPort must be 1..65535\"}}" };
		}
		
		Endpoint joinEp{ remoteIp, static_cast<uint16_t>(joinUdpPort) };
		ApiResult r = m_store.JoinRoom(roomId, joinEp);
		return { r.statusCode, "application/json; charset=utf-8", r.bodyJson };
	}

	if (req.method == "GET" && req.path == "/poll"){
		std::string roomId = GetQueryParam(req.query, "roomId");
		std::string hostToken = GetQueryParam(req.query, "hostToken");
		if (roomId.empty() || hostToken.empty()){
			return { 400, "application/json; charset=utf-8", "{\"error\":{\"code\":\"invalid_query\",\"message\":\"roomId and hostToken are required\"}}" };
		}
		ApiResult r = m_store.PollRoom(roomId, hostToken);
		return { r.statusCode, "application/json; charset=utf-8", r.bodyJson };
	}
	
	return { 404, "application/json; charset=utf-8", "{\"error\":{\"code\":\"not_found\",\"message\":\"unknown endpoint\"}}" };
}

std::string MatchingServer::BuildHttpResponse(const HttpResponse& res) {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << res.statusCode << " " << StatusText(res.statusCode) << "\r\n";
	oss << "Content-Type: " << res.contentType << "\r\n";
	oss << "Content-Length: " << res.body.size() << "\r\n";
	oss << "Connection: close\r\n";
	oss << "\r\n";
	oss << res.body;
	return oss.str();
}

bool MatchingServer::TryGetJsonIntField(const std::string& json, const std::string& key, int& outValue) {
	// 簡易的なJSONパース処理: 指定したキーの値が整数であることを前提とする
	const std::string pat = "\"" + key + "\"";
	const auto kp = json.find(pat);
	if (kp == std::string::npos) return false;
	const auto cp = json.find(':', kp + pat.size());
	if (cp == std::string::npos) return false;
	
	// コロンの後の空白をスキップしてから、整数部分を読み取る
	size_t p = cp + 1;
	while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
	
	// 整数部分を読み取る。整数は連続する数字とマイナス記号で構成されると仮定する。
	size_t e = p;
	while (e < json.size() && (std::isdigit(static_cast<unsigned char>(json[e])) || json[e] == '-')) ++e;
	if (e == p) return false;
	
	outValue = atoi(json.substr(p, e - p).c_str());
	return true;
}


bool MatchingServer::TryGetJsonStringField(const std::string & json, const std::string & key, std::string & outValue){
	const std::string pat = "\"" + key + "\"";
	const auto kp = json.find(pat);
	if (kp == std::string::npos) return false;
	const auto cp = json.find(':', kp + pat.size());
	if (cp == std::string::npos) return false;
	
	const auto q1 = json.find('"', cp + 1);
	if (q1 == std::string::npos) return false;
	const auto q2 = json.find('"', q1 + 1);
	if (q2 == std::string::npos) return false;
	
	outValue = json.substr(q1 + 1, q2 - (q1 + 1));
	return true;
}

std::string MatchingServer::GetQueryParam(const std::string& query, const std::string& key) {
	const std::string k = key + "=";
	size_t pos = 0;
	while (pos < query.size()) {
		const size_t amp = query.find('&', pos);
		const std::string part = (amp == std::string::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
		if (part.rfind(k, 0) == 0) return part.substr(k.size());
		if (amp == std::string::npos) break;
		pos = amp + 1;
	}
	return {};
}

std::string MatchingServer::StatusText(int statusCode){
	switch (statusCode){
	case 200: return "OK";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 409: return "Conflict";
	default:  return "Internal Server Error";
	}
}