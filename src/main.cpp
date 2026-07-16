// ImGui
#include "UI/UI.h"
// VIIPER
#include "VirtualController/SignalBuffer.h"
#include "VirtualController/VIIPERBackend.h"
// BLE
#include "ToyBackend/CoyoteBLEBackend.h"

#include <cstddef>
#include <mutex>
#include <vector>

int main()
{
	// ==================== Signal Buffer ====================
	SignalBuffer signalBuffer;

	// ==================== libVIIPER Init ====================
	std::cout << "[MAIN][INFO] Starting libVIIPER...\n";
	if (!VIIPERBackend::Init(
		[&signalBuffer](const std::uint8_t leftMotor, const std::uint8_t rightMotor) {
			signalBuffer.Push(leftMotor, rightMotor);
		}
	)) {
		return 1;
	}

	UI::UpdateVControllerState(true);

	std::cout
		<< "\nOpen joy.cpl to check for the controller\n"
		<< "Launch a game with controller vibration enabled\n\n";

	// ==================== BLE Init ====================
	CoyoteBLEBackend coyoteBLEBackend;

	// ==================== UI Init ====================
	if (!UI::Init())
	{
		std::cout << "[MAIN][ERROR] Failed to initialize UI\n";
		return 1;
	}
	std::cout << "[MAIN][INFO] UI initialized\n";

	std::jthread sendThread = std::jthread(
		[&](std::stop_token stopToken)
		{
			using clock = std::chrono::steady_clock;

			TimeStamp nextSendTime = clock::now();

			while (!stopToken.stop_requested())
			{
				nextSendTime += OUTPUT_INTERVAL;
				
				try
				{
					if (coyoteBLEBackend.IsConnected())
					{
						if (!coyoteBLEBackend.IsSafetyOn()) {
							// Blocking is acceptable here because
							// This is the dedicated sending thread
							const bool success =
								coyoteBLEBackend.WriteCommandAsync(
									signalBuffer.Sample<4>(OUTPUT_INTERVAL)
								).get();

							if (!success)
							{
								std::cerr << "[BLE][ERROR] Command write failed\n";
							}
						}
					}
				}
				catch (const winrt::hresult_error& error)
				{
					std::cerr
						<< "[BLE][ERROR] Command write threw: "
						<< winrt::to_string(error.message())
						<< '\n';
				}

				TimeStamp now = clock::now();
				if (now > nextSendTime) {
					nextSendTime = now;
				}
				else {
					std::this_thread::sleep_until(nextSendTime);
				}
			}
		}
	);

	// ==================== Main Loop ====================
	bool running = true;
	while (running) {
		if (!UI::Update(coyoteBLEBackend, signalBuffer)) {
			// BLE
			if (sendThread.joinable()) {
				sendThread.request_stop();
				sendThread.join();
			}

			coyoteBLEBackend.Disconnect();
			coyoteBLEBackend.StopScan();

			// VIIPER
			std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
			VIIPERBackend::Shutdown();

			// UI
			UI::Shutdown();

			running = false;
		}
	}
}