#include "matching_server.h"
#include <iostream>

int main() {
	//ローカル検証用に固定ポートで起動
	const uint16_t port = 8080;
	const std::chrono::seconds roomTtl(300);  // ルームの有効期限を5分に設定する

	MatchingServer server(port, roomTtl);
	if (!server.Run()) {
		std::cerr << "Failed to start MatchingServer." << std::endl;
		return 1;
	}
	return 0;
}