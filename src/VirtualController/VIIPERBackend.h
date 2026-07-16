#pragma once

#include <libVIIPER.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <Windows.h>

class VIIPERBackend
{
public:
	using RumbleCallback =
		std::function<void(std::uint8_t left, std::uint8_t right)>;

	static void LogCallback(
		const VIIPERLogLevel level,
		const char* message
	);

	static bool Init(RumbleCallback rumbleCallback);
	static void Shutdown();

private:
	static USBServerHandle s_Server;
	static RumbleCallback s_RumbleCallback;
};
