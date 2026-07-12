#include "VIIPERBackend.h"

int main()
{
	if (!VIIPERBackend::Init())
	{
		return 1;
	}

	std::cout
		<< "\nOpen joy.cpl to check for the controller.\n"
		<< "Launch a game with controller vibration enabled.\n\n";

	std::cin.get();

	std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
	VIIPERBackend::Shutdown();
}