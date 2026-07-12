#include <libVIIPER.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <Windows.h>


namespace
{
	void LogCallback(
		const VIIPERLogLevel level,
		const char* message
	)
	{
		const char* levelName = "UNKNOWN";

		switch (level)
		{
		case VIIPER_LOG_DEBUG: levelName = "DEBUG"; break;
		case VIIPER_LOG_INFO:  levelName = "INFO";  break;
		case VIIPER_LOG_WARN:  levelName = "WARN";  break;
		case VIIPER_LOG_ERROR: levelName = "ERROR"; break;
		}

		std::cout
			<< "[VIIPER][" << levelName << "] "
			<< (message != nullptr ? message : "")
			<< '\n';
	}

	void RumbleCallback(
		const Xbox360DeviceHandle device,
		const std::uint8_t leftMotor,
		const std::uint8_t rightMotor)
	{
		std::cout
			<< "[RUMBLE]"
			<< " left=" << static_cast<unsigned>(leftMotor)
			<< " right=" << static_cast<unsigned>(rightMotor)
			<< '\n';
	}
}

bool AddUSBipToPath()
{
	std::vector<wchar_t> buffer(32768);

	const DWORD length = GetModuleFileNameW(
		nullptr,
		buffer.data(),
		static_cast<DWORD>(buffer.size())
	);

	if (length == 0 || length >= buffer.size())
	{
		throw std::runtime_error("GetModuleFileNameW failed.");
	}

	const std::filesystem::path executableDirectory =
		std::filesystem::path(buffer.data()).parent_path();

	const std::filesystem::path usbipDirectory =
		executableDirectory / L"external" / L"USBip";

	const std::filesystem::path usbipExecutable =
		usbipDirectory / L"usbip.exe";

	if (!std::filesystem::is_regular_file(usbipExecutable))
	{
		std::wcerr
			<< L"usbip.exe not found at:\n"
			<< usbipExecutable.wstring()
			<< L'\n';

		return false;
	}

	const std::filesystem::path libusbipDll =
		usbipDirectory / L"libusbip.dll";

	if (!std::filesystem::is_regular_file(libusbipDll))
	{
		std::wcerr
			<< L"libusbip.dll not found at:\n"
			<< libusbipDll.wstring()
			<< L'\n';

		return false;
	}

	const DWORD requiredLength =
		GetEnvironmentVariableW(L"PATH", nullptr, 0);

	std::wstring currentPath;

	if (requiredLength > 0)
	{
		std::vector<wchar_t> buffer(requiredLength);

		const DWORD written = GetEnvironmentVariableW(
			L"PATH",
			buffer.data(),
			static_cast<DWORD>(buffer.size())
		);

		if (written == 0 || written >= buffer.size())
		{
			std::cerr << "[MAIN][ERROR] Could not read PATH.\n";
			return false;
		}

		currentPath.assign(buffer.data(), written);
	}

	std::wstring newPath = usbipDirectory.wstring();

	if (!currentPath.empty())
	{
		newPath += L';';
		newPath += currentPath;
	}

	if (!SetEnvironmentVariableW(L"PATH", newPath.c_str()))
	{
		std::cerr
			<< "[MAIN][ERROR] Failed to update PATH. Win32 error: "
			<< GetLastError()
			<< '\n';

		return false;
	}

	// Try resolving usbip.exe by name.
	wchar_t resolvedPath[MAX_PATH]{};

	const DWORD result = SearchPathW(
		nullptr,
		L"usbip.exe",
		nullptr,
		MAX_PATH,
		resolvedPath,
		nullptr
	);

	if (result == 0 || result >= MAX_PATH)
	{
		std::cerr
			<< "[MAIN][ERROR] Cannot locate usbip.exe in the updated PATH.\n";

		return false;
	}

	std::wcout
		<< L"[MAIN][INFO] usbip.exe resolved to:\n\t"
		<< resolvedPath
		<< L'\n';

	return true;
}

int main()
{
	std::cout << "[MAIN][INFO] Starting libVIIPER...\n";

	if (!AddUSBipToPath())
	{
		return 1;
	}

	USBServerConfig config{};
	char address[] = "127.0.0.1:3240";
	config.addr = address;

	USBServerHandle server = 0;

	if (!NewUSBServer(&config, &server, LogCallback))
	{
		std::cerr << "[MAIN][ERROR] NewUSBServer failed.\n";
		return 1;
	}

	std::cout << "[MAIN][INFO] USB server created.\n";

	std::uint32_t busId = 0;

	if (!CreateUSBBus(server, &busId))
	{
		std::cerr << "[MAIN][ERROR] CreateUSBBus failed.\n";
		CloseUSBServer(server);
		return 1;
	}

	std::cout << "[MAIN][INFO] USB bus created: " << busId << '\n';

	Xbox360DeviceHandle controller = 0;

	constexpr bool autoAttachLocalhost = true;

	if (!CreateXbox360Device(
		server,
		&controller,
		busId,
		autoAttachLocalhost,
		0,
		0,
		0))
	{
		std::cerr << "[MAIN][ERROR] CreateXbox360Device failed.\n";
		CloseUSBServer(server);
		return 1;
	}

	std::cout << "[MAIN][INFO] Virtual Xbox 360 controller created.\n";

	if (!SetXbox360RumbleCallback(controller, RumbleCallback))
	{
		std::cerr << "[MAIN][ERROR] SetXbox360RumbleCallback failed.\n";
		CloseUSBServer(server);
		return 1;
	}

	Xbox360DeviceState neutralState{};

	if (!SetXbox360DeviceState(controller, neutralState))
	{
		std::cerr << "[MAIN][ERROR] SetXbox360DeviceState failed.\n";
		CloseUSBServer(server);
		return 1;
	}

	std::cout
		<< "\nOpen joy.cpl to check for the controller.\n"
		<< "Launch a game with controller vibration enabled.\n"
		<< "Press Enter to stop.\n\n";

	std::cin.get();

	std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
	CloseUSBServer(server);

	return 0;
}