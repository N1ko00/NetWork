#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "MatchingClient.h"

#pragma comment(lib, "Ws2_32.lib")

namespace {
	bool StartsWith(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }

	std::string UrlEncodeSimple(const std::string& s) {
		//最小実装 英数字と一部記号以外を%に変換する
		std::ostringstream oss;
		for (unsigned char c : s) {
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') oss << c;
			else {
				const char* hex = "0123456789ABCDEF";
				oss << '%' << hex[c >> 4] << hex[c & 0x0F];
			}
		}
		return oss.str();
	}
}

bool MatchingClient::ParseHttpUrl(const std::string& url, std::string& outHost, uint16_t& outPort, std::string& outPathQuery, std::string& outErr) {
	//URL解析　今回は http://host:port/path のみ対象
	if (!StartsWith(url, "http://")) {
		outErr = "only http:// is supported in stage4-C";
		return false;
	}

	std::string rest = url.substr(7);  // skip "http://"
	std::string hostport;
	auto slash = rest.find('/');
	if (slash == std::string::npos) {  // パスがない場合は "/" とみなす
		hostport = rest;
		outPathQuery = "/";
	}
	else {  // パスがある場合はホストとパスを分割
		hostport = rest.substr(0, slash);
		outPathQuery = rest.substr(slash);
	}

	auto colon = hostport.find(':');
	if (colon == std::string::npos) {  // ポートがない場合は80とみなす
		outHost = hostport;
		outPort = 80;
	}
	else {  // ポートがある場合はホストとポートを分割
		outHost = hostport.substr(0, colon);
		const int p = std::atoi(hostport.substr(colon+1).c_str());
		if (p <= 0 || p > 65535) {
			outErr = "invalid port number in URL";
			return false;
		}
		outPort = static_cast<uint16_t>(p);
	}

	//ホストは空であってはならない
	if (outHost.empty()) {
		outErr = "host is empty in URL";
		return false;
	}
	return true;
}

bool MatchingClient::HttpPostJson(const std::string& url, const std::string& jsonBody, int& outStatus, std::string& outBody, std::string& outErr) {
	std::string host, pathQuery;
	uint16_t port=0;
	if (!ParseHttpUrl(url, host, port, pathQuery, outErr)) return false;
		
	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		outErr = "WSAStartup failed";
		return false;
	}

	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		outErr = "socket creation failed";
		WSACleanup();
		return false;
	}

	//ホスト名からIPアドレスを得る
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	addrinfo* res = nullptr;
	const std::string portStr = std::to_string(port);
	if (getaddrinfo(host.c_str(), portStr.c_str(),  & hints, &res) != 0 || !res) {
		outErr = "getaddrinfo failed";
		closesocket(s);
		WSACleanup();
		return false;
	}

	//得られたIPアドレスに順番に接続を試みる
	bool connected = false;
	for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
		if (::connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) {
			connected = true;
			break;
		}
	}
	freeaddrinfo(res);

	if(!connected) {
		outErr = "connect failed";
		closesocket(s);
		WSACleanup();
		return false;
	}

	//HTTPリクエストを送る
	std::ostringstream req;
	req << "POST " << pathQuery << " HTTP/1.1\r\n";
	req << "Host:" << host << ":" << port << "\r\n";
	req << "Content-Type: application/json\r\n";
	req << "Connection: close\r\n";
	req << "Content-Length: " << jsonBody.size() << "\r\n\r\n";
	req << jsonBody;

	const std::string raw = req.str();
	if(::send(s,raw.data(), static_cast<int>(raw.size()), 0) == 0) {
		outErr = "send failed";
		closesocket(s);
		WSACleanup();
		return false;
	}

	//HTTPレスポンスを受け取る
	std::string resp;
	char buf[2048];
	for (;;) {
		const int n = ::recv(s, buf, sizeof(buf), 0);
		if (n == 0) break; // connection closed
		if (n < 0) {
			outErr = "recv failed";
			closesocket(s);
			WSACleanup();
			return false;
		}
		resp.append(buf, n);
	}

	closesocket(s);
	WSACleanup();

	//HTTPレスポンスを簡単に解析する
	auto posLine = resp.find("\r\n");
	if (posLine == std::string::npos) {
		outErr = "invalid HTTP response";
		return false;
	}
	const std::string statusLine = resp.substr(0, posLine);
	{
		std::istringstream iss(statusLine);
		std::string ver;
		iss >> ver >> outStatus;
	}

	auto bodyPos = resp.find("\r\n\r\n");
	if (bodyPos == std::string::npos) {
		outBody.clear();
		return true;
	}
	outBody = resp.substr(bodyPos + 4);
	return true;
}

bool MatchingClient::HttpGet(const std::string& url, int& outStatus, std::string& outBody, std::string& outErr) {
	std::string host, pathQuery;
	uint16_t port = 0;
	if (!ParseHttpUrl(url, host, port, pathQuery, outErr)) return false;

	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		outErr = "WSAStartup failed";
		return false;
	}

	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		outErr = "socket creation failed";
		WSACleanup();
		return false;
	}

	//ホスト名からIPアドレスを得る
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	addrinfo* res = nullptr;
	const std::string portStr = std::to_string(port);
	if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
		outErr = "getaddrinfo failed";
		closesocket(s);
		WSACleanup();
		return false;
	}

	bool connected = false;
	for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
		if (::connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) {
			connected = true;
			break;
		}
	}
	freeaddrinfo(res);
	if (!connected) {
		outErr = "connect failed";
		closesocket(s);
		WSACleanup();
		return false;
	}
	
	//HTTPリクエストを送る
	std::ostringstream req;
	req << "GET " << pathQuery << " HTTP/1.1\r\n";
	req << "Host:" << host << ":" << port << "\r\n";
	req << "Connection: close\r\n\r\n";

	const std::string raw = req.str();
	if (::send(s, raw.data(), static_cast<int>(raw.size()), 0) <= 0) {
		outErr = "send failed";
		closesocket(s);
		WSACleanup();
		return false;
	}

	//HTTPレスポンスを受け取る
	std::string resp;
	char buf[2048];
	for (;;) {
		const int n = ::recv(s, buf, sizeof(buf), 0);
		if (n == 0) break; // connection closed
		if (n < 0) {
			outErr = "recv failed";
			closesocket(s);
			WSACleanup();
			return false;
		}
		resp.append(buf, buf+n);
	}

	closesocket(s);
	WSACleanup();

	//HTTPレスポンスを簡単に解析する
	auto posLine = resp.find("\r\n");
	if (posLine == std::string::npos) {
		outErr = "invalid HTTP response";
		return false;
	}
	const std::string statusLine = resp.substr(0, posLine);
	{
		std::istringstream iss(statusLine);
		std::string ver;
		iss >> ver >> outStatus;
	}

	auto bodyPos = resp.find("\r\n\r\n");
	if (bodyPos == std::string::npos) {
		outBody.clear();
		return true;
	}
	outBody = resp.substr(bodyPos + 4);
	return true;
}

bool MatchingClient::JsonGetString(const std::string& json, const std::string& key, std::string& out) {
	const std::string pat = "\"" + key + "\"";  //最小限のJSONパース　"key":"value" という形式を探す
	auto k = json.find(pat);
	if (k == std::string::npos) return false;
	auto c = json.find(':', k + pat.size());    //コロンの位置を探す
	if (c == std::string::npos) return false;
	auto q1 = json.find('"', c + 1);            //コロンの後の最初のダブルクオーテーションを探す
	if (q1 == std::string::npos) return false;
	auto q2 = json.find('"', q1 + 1);           //コロンの後の最初のダブルクオーテーションの次のダブルクオーテーションを探す
	if (q2 == std::string::npos) return false;
	out = json.substr(q1 + 1, q2 - q1 - 1);    //見つかった値を出力
	return true;
}

bool MatchingClient::JsonGetInt(const std::string& json, const std::string& key, int& out) {
	const std::string pat = "\"" + key + "\"";  //最小限のJSONパース　"key":value という形式を探す
	auto k = json.find(pat);
	if (k == std::string::npos)return false;
	auto c = json.find(':', k + pat.size());    //コロンの位置を探す
	if (c == std::string::npos)return false;
	size_t p = c + 1;
	while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p])))++p;  //コロンの後の最初の数字を探す
	size_t e = p;
	while (e < json.size() && (json[e] == '-' || std::isdigit(static_cast<unsigned char>(json[e]))))++e;  //数値の終わりを探す)
	if (e == p)return false; 
	out = std::atoi(json.substr(p, e - p).c_str());  //見つかった値を出力
	return true;
}

bool MatchingClient::JsonGetBool(const std::string& json, const std::string& key, bool& out) {
	const std::string pat = "\"" + key + "\"";  //最小限のJSONパース　"key":true/false という形式を探す
	auto k = json.find(pat);
	if (k == std::string::npos)return false;
	auto c = json.find(':', k + pat.size());    //コロンの位置を探す
	if (c == std::string::npos)return false;
	const auto t = json.find("true", c + 1);    //コロンの後のtrueを探す
	const auto f = json.find("false", c + 1);   //コロンの後のfalseを探す
	//trueとfalseの両方が見つれた場合は、より近い方を採用する
	if (t != std::string::npos && (f == std::string::npos || t < f)) {
		out = true;
		return true;
	}
	if (f != std::string::npos) {
		out = false;
		return true;
	}
	return false;
}

CreateRoomResult MatchingClient::CreateRoom(const std::string serverUrl, int myUdpPort) {
	//API呼び出し: HOSTがルームを作成
	CreateRoomResult r{};
	int status = 0;
	std::string body, err;
	const std::string url = serverUrl + "/create";
	const std::string req = std::string{ "{\"hostUdpPort\": " } + std::to_string(myUdpPort) + "\"}";
	if(!HttpPostJson(url, req, status, body, err)) {
		r.error = err;
		return r;
	}
	if (status != 200) {
		r.error = body;
		return r;
	}

	//レスポンスからroomIdとhostTokenを抜き取る
	if (!JsonGetString(body, "roomId", r.roomId) || !JsonGetString(body, "hostToken", r.hostToken)) {
		r.error = "roomId not found in response";
		return r;
	}

	r.ok = true;
	return r;
}

JoinRoomResult MatchingClient::JoinRoom(const std::string serverUrl, const std::string roomId, int myUdpPort) {
	//API呼び出し: GUESTがルームに入る
	JoinRoomResult r{};
	int status = 0;
	std::string body, err;
	const std::string url = serverUrl + "/join";
	const std::string req = std::string{ "{\"roomId\": \"" } + roomId + "\",\"joinUdpPort\":" + std::to_string(myUdpPort) + "}";
	if(!HttpPostJson(url,req,status, body, err)) {
		r.error = err;
		return r;
	}
	if(status != 200) {
		r.error = body;
		return r;
	}

	//レスポンス解析 hostendpoint.ip / port
	if(!JsonGetString(body, "ip", r.hostEndpoint.ip) || !JsonGetInt(body,"port", r.hostEndpoint.port)) {
		r.error = "ip or port not found in response";
		return r;
	}

	r.ok = true;
	return r;
}

PollRoomResult MatchingClient::PollRoom(const std::string serverUrl, const std::string roomId, const std::string hostToken) {
	//API呼び出し: HOSTがマッチング成立を待つ
	PollRoomResult r{};
	int status = 0;
	std::string body, err;
	const std::string url = serverUrl + "/poll?roomId=" + UrlEncodeSimple(roomId) + "&hostToken=" + UrlEncodeSimple(hostToken);
	if(!HttpGet(url, status, body, err)) {
		r.error = err;
		return r;
	}
	if(status != 200) {
		r.error = body;
		return r;
	}
	//レスポンス解析 matched / hostendpoint.ip / port
	bool matched = false;
	if (!JsonGetBool(body, "matched", matched)) {
		r.error = "invalid poll response";
		return r;
		
	}
	r.matched = matched;
	if (matched) {
		if (!JsonGetString(body, "ip", r.joinEndpoint.ip) || !JsonGetInt(body, "port", r.joinEndpoint.port)) {
			r.error = "invalid poll matched response";
			return r;
		}
	}
	r.ok = true;
	return r;
}