#pragma once

#include <cstdint>
#include <optional>
#include <string>

//マッチング結果で使うendpoint
struct MatchingEndpoint
{
	std::string ip;  //UDPのIPアドレス
	int port = 0;  //UDPのポート
};

//マッチングサーバーに部屋を作成してもらうためのリクエスト
struct CreateRoomResult
{
	bool ok = false;  //成功したかどうか
	std::string error;  //エラーがあればエラーメッセージ
	std::string roomId;  //部屋ID
	std::string hostToken;  //ホストトークン
};

//マッチングサーバーに部屋に入れてもらうためのリクエスト
struct JoinRoomResult
{
	bool ok = false;  //成功したかどうか
	std::string error;  //エラーがあればエラーメッセージ
	MatchingEndpoint hostEndpoint;  //ホストのUDPエンドポイント
};

//マッチングサーバーに部屋を探してもらうためのリクエスト
struct PollRoomResult
{
	bool ok = false;  //成功したかどうか
	std::string error;  //エラーがあればエラーメッセージ
	bool matched = false;  //マッチングが成立したかどうか
	MatchingEndpoint joinEndpoint;  //GUESTのUDPエンドポイント 
};

struct MatchingAutoQueueResult
{
	bool ok = false;  //成功したかどうか
	std::string error;  //エラーがあればエラーメッセージ
	std::string ticketId;  //マッチングキューのチケットID
	std::string status;  //マッチングキューの状態 "queued" "matched" "failed"
	MatchingEndpoint remoteEndpoint{};  //マッチング相手のUDPエンドポイント (status=="matched"のとき有効)
};

struct MatchingAutoPollResult
{
	bool ok = false;
	std::string error;
	std::string status;
	MatchingEndpoint remoteEndpoint{};
};

class MatchingClient {
public:
	//createを呼び、roomIdとhostTokenを受け取る
	CreateRoomResult CreateRoom(const std::string serverUrl, int myUdpPort);

	//joinを呼び、hostのendpointを受け取る
	JoinRoomResult JoinRoom(const std::string serverUrl, const std::string roomId, int myUdpPort);

	//pollを呼び、matchedとjoinendpointを受け取る
	PollRoomResult PollRoom(const std::string serverUrl, const std::string roomId, const std::string hostToken);

	//cancelを呼び、host側待機をキャンセルする
	bool CancelRoom(const std::string serverUrl, const std::string roomId, const std::string hostToken, std::string& outErr);

	//Auto Matching用API　Queueを呼び、ticketIdとstatusを受け取る
	bool QueueAutoMatching(const std::string serverUrl, int myUdpPort, MatchingAutoQueueResult& out);
	
	//Auto Matching用API Pollを呼び、statusとremoteEndpointを受け取る
	bool PollAutoMatching(const std::string serverUrl, const std::string & ticketId, MatchingAutoPollResult & out);
	
	//Auto Matching用API Cancelを呼び、マッチングキューをキャンセルする
	bool CancelAutoMatching(const std::string serverUrl, const std::string & ticketId, std::string & outErr);
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

	
	std::string ExtractErrorCode(const std::string& body);
};