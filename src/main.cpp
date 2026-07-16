// ImGui
#include "UI/UI.h"
// VIIPER
#include "VirtualBackend/SignalBuffer.h"
#include "VirtualBackend/VIIPERBackend.h"
// BLE
#include "BLE/CoyoteBLEClient.h"

#include <cstddef>
#include <mutex>
#include <vector>

int main()
{
	// ==================== Signal Buffer ====================
	SignalBuffer signalBuffer(std::chrono::milliseconds(500));

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

	// ==================== UI Init ====================
	if (!UI::Init())
	{
		std::cout << "[MAIN][ERROR] Failed to initialize UI\n";
		return 1;
	}
	std::cout << "[MAIN][INFO] UI initialized\n";

	// ==================== Main Loop ====================
	bool running = true;
	while (running) {
		coyoteBLEClient.UpdateAdvertisements();
		signalBuffer.UpdateSnapshot();

		if (!UI::Update(coyoteBLEClient, signalBuffer))
		{
			UI::Shutdown();
			running = false;
		}
	}

	coyoteBLEClient.StopScan();

	//std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
	//VIIPERBackend::Shutdown();
}