// VIIPER
#include "VIIPERBackend.h"
// ImGui
#include "UI.h"

int main()
{
	// ==================== UI Init ====================
	if (!UI::Init())
	{
		std::cout << "[MAIN][ERROR] Failed to initialize UI\n";
		return 1;
	}
	std::cout << "[MAIN][INFO] UI initialized\n";

	// ==================== libVIIPER Init ====================
	std::cout << "[MAIN][INFO] Starting libVIIPER...\n";
	if (!VIIPERBackend::Init())
	{
		return 1;
	}

	UI::UpdateAppState(true, false);

	std::cout
		<< "\nOpen joy.cpl to check for the controller\n"
		<< "Launch a game with controller vibration enabled\n\n";


	// ==================== Main Loop ====================
	bool running = true;
	while (running) {
		if (!UI::Update()) {
			UI::Shutdown();
			running = false;
		}
	}

	std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
	VIIPERBackend::Shutdown();
}