// Waveform
#include "SignalBuffer.h"
// ImGui
#include "UI.h"
// VIIPER
#include "VIIPERBackend.h"
// BLE
#include "CoyoteBLEClient.h"

#include <cstddef>
#include <mutex>
#include <vector>

int main()
{
	//// ==================== Signal Buffer ====================
	//SignalBuffer buffer(std::chrono::milliseconds(500));

	//// ==================== UI Init ====================
	//if (!UI::Init())
	//{
	//	std::cout << "[MAIN][ERROR] Failed to initialize UI\n";
	//	return 1;
	//}
	//std::cout << "[MAIN][INFO] UI initialized\n";

	//// ==================== libVIIPER Init ====================
	//std::cout << "[MAIN][INFO] Starting libVIIPER...\n";
	//if (!VIIPERBackend::Init(
	//	[&buffer](const std::uint8_t leftMotor, const std::uint8_t rightMotor) {
	//		buffer.Push(leftMotor, rightMotor);
	//	}
	//)) {
	//	return 1;
	//}

	//UI::UpdateVControllerState(true);

	//std::cout
	//	<< "\nOpen joy.cpl to check for the controller\n"
	//	<< "Launch a game with controller vibration enabled\n\n";

	// ==================== BLE Init ====================
	CoyoteBLEClient coyoteBLEClient;

	coyoteBLEClient.StartScan();

	std::cout << "[BLE][INFO] Scanning... (Press Enter to stop)\n";
	std::cin.get();

	coyoteBLEClient.StopScan();

	//// ==================== Main Loop ====================
	//bool running = true;
	//while (running) {
	//	if (!UI::Update(buffer.GetSnapshot(), std::chrono::milliseconds(2000))) {
	//		UI::Shutdown();
	//		running = false;
	//	}
	//}

	//std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
	//VIIPERBackend::Shutdown();
}