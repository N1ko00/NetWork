#pragma once

#include <cstdint>
#include <string>

struct RuntimeConnectionSettings
{
	bool enabled = true;
	int myPort = 50001;
	std::string remoteIp = "127.0.0.1";
	int remotePort = 50000;
	uint64_t machineId = 2;

	//Matching用の最小メタ情報　UI状態保持とデバック用
	std::string matchingServerUrl = "http://127.0.0.1:8080";
	std::string matchingServerIp = "127.0.0.1";
	int matchingServerPort = 8080;
	std::string roomId{};
	std::string hostToken{};
};

class ConnectionSettingsStore
{
public:
	static RuntimeConnectionSettings & Mutable()
		 {
		static RuntimeConnectionSettings s_settings{};
		return s_settings;
	}
	
	static const RuntimeConnectionSettings & Get()
	{
		return Mutable();
	}
};