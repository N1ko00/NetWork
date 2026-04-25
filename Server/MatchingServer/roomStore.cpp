#include "room_store.h"

#include <iostream>
#include <random>
#include <sstream>

//ルームIDとトークンはランダムな文字列で生成する
namespace {
	std::string NowTag() {
		return "[MatchingServer}";
	}
}

RoomStore::RoomStore(std::chrono::seconds ttl) : m_ttl(ttl) {}

//ルームを作成し、ルームIDとホストトークンを返す
ApiResult RoomStore::CreateRoom(const Endpoint& hostEP) {
	std::lock_guard<std::mutex> lock(m_mutex);  
	CleanupExpiredRoomsLocked();

	//ルームIDとトークンを生成
	//HostのUDP受信ポートとHTTP送信元IDから得たEndpointを保存する
	Room room;
	room.roomId = MakeRoomId();
	while (m_rooms.find(room.roomId) != m_rooms.end()) {
		room.roomId = MakeRoomId();
	}
	room.hostToken = MakeToken();
	room.hostEndpoint = hostEP;
	room.expiresAt = std::chrono::steady_clock::now() + m_ttl;

	m_rooms.emplace(room.roomId, room);

	//ルーム作成のログを出力
	std::cout << NowTag() << " Created roomId " << room.roomId << 
		" for host " << hostEP.ip << ":" << hostEP.port << std::endl;

	//ルームIDとホストトークンをJSON形式で返す
	std::ostringstream oss;
	oss<<"{"
		<<"\"roomId\":\""<<EscapeJson(room.roomId)<<"\","
		<< "\"hostToken\":" << EscapeJson(room.hostToken)<<"\""
		<< "}";
	return { 200, oss.str() };
}

//ルームに参加し、ホストのエンドポイントを返す
ApiResult RoomStore::JoinRoom(const std::string& roomId, const Endpoint& joinEp) {
	std::lock_guard<std::mutex> lock(m_mutex);
	CleanupExpiredRoomsLocked();

	auto it = m_rooms.find(roomId);
	//ルームが見つからない場合は404エラーを返す
	if(it==m_rooms.end()) {
		return Error(404, "room_not_found", "Room not found");
	}

	Room& room = it->second;

	//join処理
	//1.room=2人までしか参加できない
	if(room.joinEndpoint.has_value()) {
		return Error(409, "room_full", "Room is full");
	}

	room.joinEndpoint = joinEp;
	room.expiresAt = std::chrono::steady_clock::now() + m_ttl;

	//ルーム参加のログを出力
	std::cout << NowTag() << " join roomId=" << room.roomId << " join=" << joinEp.ip <<
		":" << joinEp.port << std::endl;
	std::cout << NowTag() << " match roomId=" << room.roomId
		 << " host=" << room.hostEndpoint.ip << ":" << room.hostEndpoint.port
		 << " join=" << joinEp.ip << ":" << joinEp.port << std::endl;

	//ホストのエンドポイントをJSON形式で返す
	std::ostringstream oss;
	oss << "{"
		<< "\"hostEndpoint\":{"
		<< "\"ip\":\"" << EscapeJson(room.hostEndpoint.ip) << "\","
		<< "\"port\":" << room.hostEndpoint.port
		<< "}"
		<< "}";
	return { 200, oss.str() };
}