#include "VIIPERBackend.h"

USBServerHandle VIIPERBackend::s_Server = 0;
VIIPERBackend::RumbleCallback VIIPERBackend::s_RumbleCallback;

void VIIPERBackend::LogCallback(
	const VIIPERLogLevel level,
	const char* message
) {
	const char* levelName = "UNKNOWN";

	switch (level)
	{
#ifdef TC_DEBUG
	case VIIPER_LOG_DEBUG:
		levelName = "DEBUG";
		break;
#endif // TC_DEBUG
	case VIIPER_LOG_INFO:
		levelName = "INFO";
		break;
	case VIIPER_LOG_WARN:
		levelName = "WARN";
		break;
	case VIIPER_LOG_ERROR:
		levelName = "ERROR";
		break;
	default:
		return;
	}

	std::cout
		<< "[VIIPER][" << levelName << "] "
		<< (message != nullptr ? message : "")
		<< '\n';
}

bool VIIPERBackend::Init(RumbleCallback rumbleCallback) {
	s_RumbleCallback = std::move(rumbleCallback);

	// ==================== Resolve PATH ====================
	std::vector<wchar_t> buffer(32768);

	const DWORD length = GetModuleFileNameW(
		nullptr,
		buffer.data(),
		static_cast<DWORD>(buffer.size())
	);

	if (length == 0 || length >= buffer.size())
	{
		std::wcerr << L"[VIIPER][ERROR] GetModuleFileNameW failed.\n";

		return false;
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
			<< L"[VIIPER][ERROR] usbip.exe not found at:\n"
			<< usbipExecutable.wstring()
			<< L'\n';

		return false;
	}

	const std::filesystem::path libusbipDll =
		usbipDirectory / L"libusbip.dll";

	if (!std::filesystem::is_regular_file(libusbipDll))
	{
		std::wcerr
			<< L"[VIIPER][ERROR] libusbip.dll not found at:\n"
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
			std::cerr << "[VIIPER][ERROR] Could not read PATH.\n";
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
			<< "[VIIPER][ERROR] Failed to update PATH. Win32 error: "
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
			<< "[VIIPER][ERROR] Cannot locate usbip.exe in the updated PATH.\n";

		return false;
	}

	std::wcout
		<< L"[VIIPER][INFO] usbip.exe resolved to:\n\t"
		<< resolvedPath
		<< L'\n';

	// ==================== Start USBip ====================
	std::cout << "[VIIPER][INFO] Starting libVIIPER...\n";

	USBServerConfig config{};
	char address[] = "127.0.0.1:3240";
	config.addr = address;

	if (!NewUSBServer(&config, &s_Server, VIIPERBackend::LogCallback))
	{
		std::cerr << "[VIIPER][ERROR] NewUSBServer failed.\n";
		return false;
	}

	std::cout << "[VIIPER][INFO] USB server created.\n";

	std::uint32_t busId = 0;

	if (!CreateUSBBus(s_Server, &busId))
	{
		std::cerr << "[VIIPER][ERROR] CreateUSBBus failed.\n";
		CloseUSBServer(s_Server);
		return false;
	}

	std::cout << "[VIIPER][INFO] USB bus created: " << busId << '\n';

	Xbox360DeviceHandle controller = 0;

	constexpr bool autoAttachLocalhost = true;

	if (!CreateXbox360Device(
		s_Server,
		&controller,
		busId,
		autoAttachLocalhost,
		0,
		0,
		0))
	{
		std::cerr << "[VIIPER][ERROR] CreateXbox360Device failed.\n";
		CloseUSBServer(s_Server);
		return false;
	}

	std::cout << "[VIIPER][INFO] Virtual Xbox 360 controller created.\n";

	if (!SetXbox360RumbleCallback(
		controller,
		[](const Xbox360DeviceHandle _,	const std::uint8_t leftMotor, const std::uint8_t rightMotor)
		{
			s_RumbleCallback(leftMotor, rightMotor);

			std::cout
				<< "[RUMBLE]"
				<< " left=" << static_cast<unsigned>(leftMotor)
				<< " right=" << static_cast<unsigned>(rightMotor)
				<< '\n';
		}
	)) {
		std::cerr << "[VIIPER][ERROR] SetXbox360RumbleCallback failed.\n";
		CloseUSBServer(s_Server);
		return false;
	}

	Xbox360DeviceState neutralState{};

	if (!SetXbox360DeviceState(controller, neutralState))
	{
		std::cerr << "[VIIPER][ERROR] SetXbox360DeviceState failed.\n";
		CloseUSBServer(s_Server);
		return false;
	}

	return true;
}

void VIIPERBackend::Shutdown() {
	CloseUSBServer(s_Server);
}