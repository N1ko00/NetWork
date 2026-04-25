#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

struct Endpoint {
	std::string ip;
	uint16_t port = 0;
};

struct ApiResult {
	int statusCode = 500;
	std::string bodyJson;
};

class RoomStore {
public:
	explicit RoomStore(std::chrono::seconds ttl);   // ルームの有効期限

	ApiResult CreateRoom(const Endpoint& hostEP);   // ルームを作成し、ルームIDとホストトークンを返す
	ApiResult JoinRoom(const std::string& roomId, const Endpoint& joinEp);   // ルームに参加し、ホストのエンドポイントを返す
	ApiResult pollRoom(const std::string& roomId, const std::string& hostToken);   // ホストがルームの状態を確認するためのAPI。参加者がいる場合は参加者のエンドポイントを返す

private:
	struct Room {
		std::string roomId;
		std::string hostToken;
		Endpoint hostEndpoint{};
		std::optional<Endpoint> joinEndpoint{};
		std::chrono::steady_clock::time_point expiresAt;
	};

	std::string MakeRoomId();
	std::string MakeToken();
	void CleanupExpiredRoomsLocked();

	static std::string EscapeJson(const std::string& s);
	static ApiResult Error(int statusCode, const std::string& code, const std::string& message);

	std::chrono::seconds m_ttl;
	std::mutex m_mutex;
	std::unordered_map<std::string, Room> m_rooms;
};