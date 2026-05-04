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
}