#include "roomStore.h"

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
		<< "\"hostToken\":\"" << EscapeJson(room.hostToken)<<"\""
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

ApiResult RoomStore::PollRoom(const std::string& roomId, const std::string& hostToken) {
	std::lock_guard<std::mutex> lock(m_mutex);
	CleanupExpiredRoomsLocked();

	auto it = m_rooms.find(roomId);
	if (it == m_rooms.end()) {
		return Error(404, "room_not_found", "Room not found");
	}

	Room& room = it->second;
	if (room.hostToken != hostToken) {
		return Error(403, "invalid_host_token", "Invalid host token");
	}

	//Poll処理
	//Hostが参加待ち中かJoin成立済みかを確認する
	if (!room.joinEndpoint.has_value()) {
		return { 200,"{\matchined\":false" };
	}

	const Endpoint& joinEp = *room.joinEndpoint;
	std::ostringstream oss;
	oss << "{"
		 << "\"matched\":true,"
		 << "\"joinEndpoint\":{"
		 << "\"ip\":\"" << EscapeJson(joinEp.ip) << "\","
		 << "\"port\":" << joinEp.port
		 << "}"
		 << "}";
	return { 200, oss.str() };
}

std::string RoomStore::MakeRoomId() {
	static thread_local std::mt19937 rng(std::random_device{}());   // 乱数生成器
	static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";  // ルームIDに使用する文字のテーブル
	std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(kTable) - 2));  // 0からテーブルのサイズ-2までの範囲で乱数を生成

	std::string id;
	id.reserve(6);  // 6文字のルームIDを生成
	for (int i = 0; i < 6; ++i) {
		id.push_back(kTable[dist(rng)]);
	}
	return id;
}

std::string RoomStore::MakeToken() {
	static thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<unsigned long long> dist;  // 0からunsigned long longの最大値までの範囲で乱数を生成
	unsigned long long x = dist(rng);
	unsigned long long y = dist(rng);
	std::ostringstream oss;  // 16進数で64ビットの乱数を2つ連結して128ビットのトークンを生成
	oss << std::hex << x << y;
	return oss.str();
}

void RoomStore::CleanupExpiredRoomsLocked() {
	//TTLが切れたルームを削除する
	//放置されたルームを削除し、メモリリークと古いルームへの参加を防止する
	const auto now = std::chrono::steady_clock::now();	
	for (auto i = m_rooms.begin(); i != m_rooms.end();) {
		if (i->second.expiresAt <= now) {  //ルームの有効期限が切れている場合は削除する
			std::cout << NowTag() << " Expired roomId=" << i->second.roomId << std::endl;  //ルームの有効期限切れのログを出力
			i = m_rooms.erase(i);  //ルームを削除し、次のルームに進む
		} else {
			++i;
		}
	}
}

std::string RoomStore::EscapeJson(const std::string& s) {
	std::string out;
	out.reserve(s.size());  // 出力文字列のサイズを入力文字列と同じに予約する
	for (char c : s) {
		if (c == '\\')out += "\\\\";  // バックスラッシュはエスケープする
		else if (c == '"')out += "\\\"";  // ダブルクォートはエスケープする
		else out += c;  // その他の文字はそのまま出力する
	}
	return out;
}

ApiResult RoomStore::Error(int statusCode, const std::string& code, const std::string& message) {
	std::ostringstream oss;
	oss << "{"
		<< "\"error\":{"
		<< "\"code\":\"" << EscapeJson(code) << "\","
		<< "\"message\":\"" << EscapeJson(message) << "\""
		<< "}"
		<< "}";
	return { statusCode, oss.str() };
}