// ImGui
#include "UI/UI.h"
// VIIPER
#include "VirtualController/VIIPERBackend.h"
// Audio capture
#include "Audio/WASAPICapture.h"
// BLE
#include "ToyBackend/CoyoteBLEBackend.h"

#include "SignalBuffer.h"

#include <cstddef>
#include <mutex>
#include <vector>

constexpr TimeDuration HISTORY_SPAN = std::chrono::milliseconds(2000);
constexpr TimeDuration HISTORY_STRIDE = std::chrono::milliseconds(50);
constexpr ImVec4 WAVEFORM_COLOR_L(0.26f, 0.53f, 0.96f, 0.9f);
constexpr ImVec4 WAVEFORM_COLOR_R(0.96f, 0.69f, 0.26f, 0.9f);

int main()
{
	constexpr InputMode g_InputMode = InputMode::SystemAudio;

	// ==================== Signal Buffer ====================
	SignalBuffer signalBuffer;

	// ==================== Virtual Controller ====================
	if (g_InputMode == InputMode::VirtualController) {
		std::cout << "[MAIN][INFO] Starting libVIIPER...\n";
		if (!VIIPERBackend::Init(
			[&signalBuffer](const std::uint8_t leftMotor, const std::uint8_t rightMotor) {
				signalBuffer.Push(
					static_cast<float>(leftMotor) / 255.0f,
					static_cast<float>(rightMotor) / 255.0f
				);
			}
		)) {
			return 1;
		}

		std::cout
			<< "\nOpen joy.cpl to check for the controller\n"
			<< "Launch a game with controller vibration enabled\n\n";
	}
	// ==================== System Audio ====================
	else {
		std::cout << "[MAIN][INFO] Starting WASAPI...\n";
		if (!WASAPICapture::Start([&signalBuffer](float l, float r) {
			signalBuffer.Push(l, r);
			})) {
			std::cerr << "[MAIN][ERROR] Failed to start WASAPI capture\n";
			return 1;
		}

		while (true) {
			if (WASAPICapture::IsRunning() == 1) {
				break;
			}
			if (WASAPICapture::IsRunning() == -1) {
				std::cerr << "[MAIN][ERROR] WASAPI capture stopped unexpectedly\n";
				return 1;
			}
		}
	}

	// ==================== BLE Init ====================
	CoyoteBLEBackend coyoteBLEBackend;

	// ==================== UI Init ====================
	if (!UI::Init())
	{
		std::cout << "[MAIN][ERROR] Failed to initialize UI\n";
		return 1;
	}
	std::cout << "[MAIN][INFO] UI initialized\n";

	// ==================== Waveform Sending Thread ====================
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
					if (coyoteBLEBackend.IsPulseUnitConnected())
					{
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
	while (true) {
		if (!UI::BeginFrame()) break;
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();

			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);

			ImGui::Begin(
				"Main",
				nullptr,
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings
			);
			{
				// ========== Connectivity ==========
				bool toyConnected = coyoteBLEBackend.IsPulseUnitConnected();
				if (toyConnected)
				{
					ImGui::Text("Toy: Connected");
				}
				else
				{
					ImGui::Text("Toy: Disconnected");
				}

				ImGui::SameLine();

				if (g_InputMode == InputMode::SystemAudio)
				{
					ImGui::SetCursorPosX(
						ImGui::GetWindowContentRegionMax().x -
						250.0f -
						ImGui::GetStyle().ItemSpacing.x
					);
					if (ImGui::Button("Restart Audio Capturer", { 160.0f , 0.0f })) {
						WASAPICapture::Restart();
					};

					ImGui::SameLine();
				}

				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 90.0f);
				const char* itemID = "Device List Popup";
				if (ImGui::Button("Device List", { 90.0f , 0.0f })) {
					ImGui::OpenPopup(itemID);
					coyoteBLEBackend.StartScan();
				};
				ImVec2 rectMax = ImGui::GetItemRectMax();
				ImGui::SetNextWindowPos({
					rectMax.x - 500.0f,
					rectMax.y + ImGui::GetStyle().ItemSpacing.y
					});
				ImGui::SetNextWindowSize({ 500.0f, 0.0f });
				ImGui::SetNextWindowSizeConstraints(
					{ 500.0f, 300.0f },
					{ 500.0f, FLT_MAX }
				);
				if (ImGui::BeginPopup(
					itemID,
					ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoSavedSettings
				)) {
					if (ImGui::BeginTable("Device Lists", 2)) {
						ImGui::TableSetupColumn(
							"Connectable",
							ImGuiTableColumnFlags_WidthStretch,
							1.0f
						);
						ImGui::TableSetupColumn(
							"Connected",
							ImGuiTableColumnFlags_WidthStretch,
							1.0f
						);

						ImGui::TableNextColumn();
						{
							ImGui::Text("Connected");
							ImGui::Separator();
							if (toyConnected) {
								ImGui::Text("Coyote Pulse Unit");
							}
							if (coyoteBLEBackend.IsPawPrintConnected()) {
								ImGui::Text("PawPrint Button");
							}
						}
						ImGui::TableNextColumn();
						{
							const std::unordered_map<std::uint64_t, BLEAdvertisementInfo> advertisements =
								coyoteBLEBackend.GetAdvertisements();
							if (advertisements.empty())
							{
								UI::TextCentered("No bluetooth devices found", true);
							}
							else
							{
								ImGui::TextUnformatted("Nearby Devices");
								ImGui::Separator();
								for (const std::pair<std::uint64_t, BLEAdvertisementInfo>& p : advertisements)
								{
									ImGui::Text("%s", p.second.name.c_str());
									ImGui::Text("RSSI: %d dBm", p.second.rssi);

									if (p.second.connectionState == BLEConnectionState::Connected)
									{
										UI::TextRAligned("Connected");
									}
									else if (p.second.connectionState == BLEConnectionState::Connecting)
									{
										ImGui::BeginDisabled();
										UI::ButtonRAligned(std::format("Connecting...##{}", p.first).c_str(), 0.0f);
										ImGui::EndDisabled();
									}
									else if (UI::ButtonRAligned(std::format("Connect##{}", p.first).c_str(), 0.0f))
									{
										coyoteBLEBackend.ConnectAsync(p.first);
									}
								}
							}
						}

						ImGui::EndTable();
					}

					ImGui::EndPopup();
				}
				else {
					coyoteBLEBackend.StopScan();
				}

				ImGui::Separator();

				// ========== Side Panel ==========
				ImGui::BeginChild(
					"Controls",
					ImVec2(250.0f, 0.0f),
					ImGuiChildFlags_Borders
				);

				if (toyConnected)
				{
					// ===== Safety Button =====
					if (coyoteBLEBackend.IsSafetyOn())
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.52f, 0.52f, 0.52f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.62f, 0.31f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.49f, 0.19f, 1.0f));

						if (ImGui::Button("RESUME", ImVec2(-1.0f, 70.0f)))
						{
							coyoteBLEBackend.SetSafety(false);
						}

						ImGui::PopStyleColor(3);
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.37f, 0.26f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.42f, 0.31f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.63f, 0.18f, 0.18f, 1.0f));

						if (ImGui::Button("STOP", ImVec2(-1.0f, 70.0f)))
						{
							coyoteBLEBackend.SetSafety(true);
						}

						ImGui::PopStyleColor(3);
					}

					ImGui::Separator();
					// ===== Strength =====
					float strength = signalBuffer.GetStrengthAmp();

					if (ImGui::Button("+", ImVec2(-1.0f, 0.0f)))
					{
						strength += 1.0f;
					}
					char textContent[15];
					std::snprintf(textContent, 15, "Strength: %.0f", strength);
					UI::TextCentered(textContent);
					if (ImGui::Button("-", ImVec2(-1.0f, 0.0f)))
					{
						strength = max(0.0f, strength - 1.0f);
					}

					signalBuffer.SetStrengthAmp(strength);

					ImGui::Separator();
					// ===== Disconnect =====
					if (ImGui::Button("Disconnect", ImVec2(-1.0f, 0.0f)))
					{
						coyoteBLEBackend.DisconnectPulseUnit();
					}
				}
				else
				{
					UI::TextCentered("No Toy Connected", true);
				}

				ImGui::EndChild();

				ImGui::SameLine();
				// ========== Signals ==========
				ImGui::BeginChild(
					"Visualization",
					ImVec2(0.0f, 0.0f)
				);

				size_t numPlotPoints;
				if (g_InputMode == InputMode::VirtualController) {
					numPlotPoints = static_cast<size_t>(HISTORY_SPAN / HISTORY_STRIDE);
				}
				else {
					numPlotPoints = BUFFER_SIZE;
				}
				std::vector<float> leftChSignals(numPlotPoints);
				std::vector<float> rightChSignals(numPlotPoints);

				SignalBufferSnapshot signalBufferSnapshot = signalBuffer.GetSnapshot();
				const std::vector<Signal>& signals = *signalBufferSnapshot;
				if (g_InputMode == InputMode::VirtualController) {
					if (signals.size() > 0) {
						TimeStamp startTime = std::chrono::steady_clock::now() - HISTORY_SPAN;
						for (size_t i = signals.size() - 1; i > 0; i--)
						{
							if (signals[i].time < startTime)
							{
								break;
							}

							TimeStamp t = max(signals[i - 1].time, startTime);
							size_t index =
								static_cast<size_t>((t - startTime) / HISTORY_STRIDE);
							for (; t < signals[i].time; t += HISTORY_STRIDE)
							{
								if (index >= numPlotPoints) break;
								leftChSignals[index] = signals[i - 1].left;
								rightChSignals[index] = signals[i - 1].right;
								index++;
							}
						}
						Signal lastSignal = signals.back();
						TimeStamp t = lastSignal.time;
						size_t index = static_cast<size_t>((t - startTime) / HISTORY_STRIDE);
						for (; t < std::chrono::steady_clock::now(); t += HISTORY_STRIDE)
						{
							if (index >= numPlotPoints) break;
							leftChSignals[index] = lastSignal.left;
							rightChSignals[index] = lastSignal.right;
							index++;
						}
					}
				}
				else if (g_InputMode == InputMode::SystemAudio) {
					for (size_t i = 0; i < numPlotPoints; i++) {
						if (i >= signals.size()) break;
						leftChSignals[i] = signals[i].left;
						rightChSignals[i] = signals[i].right;
					}
				}

				ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
				if (ImPlot::BeginPlot(
					"Waveforms",
					{ -1.0f, 150.0f },
					ImPlotFlags_NoFrame |
					ImPlotFlags_NoMouseText
				)) {
					ImPlot::SetupAxesLimits(
						0.0, numPlotPoints - 1,
						-0.05, 1.05,
						ImGuiCond_Always
					);
					ImPlot::SetupAxes(
						nullptr,
						nullptr,
						ImPlotAxisFlags_NoDecorations,
						ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock
					);

					ImGui::PushStyleColor(ImGuiCol_PlotLines, WAVEFORM_COLOR_L);
					ImPlot::PlotLine("##LSignals", leftChSignals.data(), numPlotPoints);
					ImGui::PushStyleColor(ImGuiCol_PlotLines, WAVEFORM_COLOR_R);
					ImPlot::PlotLine("##RSignals", rightChSignals.data(), numPlotPoints);
					ImGui::PopStyleColor(2);

					ImPlot::EndPlot();
				}
				ImPlot::PopStyleVar();

				ImGui::ColorButton(
					"##LeftColor",
					WAVEFORM_COLOR_L,
					ImGuiColorEditFlags_NoTooltip |
					ImGuiColorEditFlags_NoDragDrop,
					ImVec2(12.0f, 12.0f)
				);
				ImGui::SameLine();
				ImGui::TextUnformatted("Left Output");
				ImGui::SameLine();
				ImGui::ColorButton(
					"##RightColor",
					WAVEFORM_COLOR_R,
					ImGuiColorEditFlags_NoTooltip |
					ImGuiColorEditFlags_NoDragDrop,
					ImVec2(12.0f, 12.0f)
				);
				ImGui::SameLine();
				ImGui::TextUnformatted("Right Output");

				ImGui::EndChild();
			}
			ImGui::End();
		}
		UI::EndFrame();
	}

	// =================== Shutdown  ====================
	// BLE
	if (sendThread.joinable()) {
		sendThread.request_stop();
		sendThread.join();
	}

	coyoteBLEBackend.DisconnectPulseUnit();
	coyoteBLEBackend.StopScan();

	// Stop audio capture or VIIPER depending on mode
	if (g_InputMode == InputMode::SystemAudio) {
		std::cout << "[MAIN][INFO] Closing WASAPICapturer...\n";
		WASAPICapture::Stop();
	}
	else {
		std::cout << "[MAIN][INFO] Closing libVIIPER...\n";
		VIIPERBackend::Shutdown();
	}

	// UI
	UI::Shutdown();
}