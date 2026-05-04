#pragma once

#include <cstdint>
#include <optional>
#include <string>

//マッチング結果で使うendpoint
struct MatchingEndpoint
{
	std::string ip;
	int port = 0;
};

//マッチングサーバーに部屋を作成してもらうためのリクエスト
struct CreateRoomResult
{
	bool ok = false;
	std::string error;
	std::string roomId;
	std::string hostToken;
};

//マッチングサーバーに部屋に入れてもらうためのリクエスト
struct JoinRoomResult
{
	bool ok = false;
	std::string error;
	MatchingEndpoint hostEndpoint;
};

//マッチングサーバーに部屋を探してもらうためのリクエスト
struct PollRoomResult
{
	bool ok = false;
	std::string error;
	bool matched = false;
	MatchingEndpoint hostEndpoint;
};

class MatchingClient {
public:
	//createを呼び、roomIdとhostTokenを受け取る
	CreateRoomResult CreateRoom(const std::string serverUrl, int myUdpPort);

	//joinを呼び、hostのendpointを受け取る
	JoinRoomResult JoinRoom(const std::string serverUrl, const std::string roomId, int myUdpPort);

	//pollを呼び、matchedとjoinendpointを受け取る
	PollRoomResult PollRoom(const std::string serverUrl, const std::string roomId, const std::string hostToken);

private:
	//最小HTTP POST(JSON)
	bool HttpPostJson(const std::string& url, const std::string& jsonBody, int& outStatus, std::string& outBody, std::string& outErr);

	//最小HTTP GET
	bool HttpGet(const std::string& url, int& outStatus, std::string& outBody, std::string& outErr);
	
	//URL "http://host:port/path?query" を簡易分解する
	bool ParseHttpUrl(const std::string& url, std::string& outHost, uint16_t& outPort, std::string& outPathQuery, std::string& outErr);

	//最小JSON抽出
	bool JsonGetString(const std::string& json, const std::string& key, std::string& out);
	bool JsonGetInt(const std::string& json, const std::string& key, int& out);
	bool JsonGetBool(const std::string & json, const std::string & key, bool& out);
};