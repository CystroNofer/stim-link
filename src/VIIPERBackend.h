#pragma once

#include <libVIIPER.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <Windows.h>

class VIIPERBackend
{
public:
	static void LogCallback(
		const VIIPERLogLevel level,
		const char* message
	);

	static void RumbleCallback(
		const Xbox360DeviceHandle device,
		const std::uint8_t leftMotor,
		const std::uint8_t rightMotor
	);

	static bool Init();
	static void Shutdown();

private:
	static USBServerHandle server;
};
