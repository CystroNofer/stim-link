#include "UI.h"

#include <iterator>

ID3D11Device* g_Device = nullptr;
ID3D11DeviceContext* g_DeviceContext = nullptr;
IDXGISwapChain* g_SwapChain = nullptr;
ID3D11RenderTargetView* g_RenderTargetView = nullptr;

HINSTANCE UI::m_HInstance = nullptr;
const wchar_t* UI::m_WindowClassName = L"CoyoteControllerWindow";
HWND UI::m_Window = nullptr;

bool UI::m_VirtualControllerConnected = false;

constexpr TimeDuration HISTORY_STRIDE = std::chrono::milliseconds(50);
constexpr ImVec4 WAVEFORM_COLOR_L(0.26f, 0.53f, 0.96f, 0.9f);
constexpr ImVec4 WAVEFORM_COLOR_R(0.96f, 0.69f, 0.26f, 0.9f);

void TextCentered(const char* textContent, bool yCentered = false) {
	if (yCentered) {
		ImVec2 curr = ImGui::GetCursorPos();
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 textSize = ImGui::CalcTextSize(textContent);
		ImGui::SetCursorPos({
			curr.x + (avail.x - textSize.x) * 0.5f,
			curr.y + (avail.y - textSize.y) * 0.5f
			});
		ImGui::TextUnformatted(textContent);
	}
	else {
		float avail = ImGui::GetContentRegionAvail().x;
		float textSize = ImGui::CalcTextSize(textContent).x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - textSize) * 0.5f);
		ImGui::TextUnformatted(textContent);
	}
}

void TextRAligned(const char* textContent) {
	ImGui::SetCursorPosX(
		ImGui::GetCursorPosX() +
		ImGui::GetContentRegionAvail().x -
		ImGui::CalcTextSize(textContent).x
	);
	ImGui::TextUnformatted(textContent);
}

bool ButtonRAligned(const char* textContent, float width) {
	if (width == 0.0f) {
		width =
			ImGui::CalcTextSize(textContent).x +
			ImGui::GetStyle().FramePadding.x * 2.0f;
	}
	ImGui::SetCursorPosX(
		ImGui::GetCursorPosX() +
		ImGui::GetContentRegionAvail().x -
		width
	);
	return ImGui::Button(textContent, { width, 0.0f });
}

void CreateRenderTarget()
{
	ID3D11Texture2D* backBuffer = nullptr;

	g_SwapChain->GetBuffer(
		0,
		IID_PPV_ARGS(&backBuffer)
	);

	if (!backBuffer) {
		std::cerr << "[UI][ERROR] CreateRenderTarget failed\n";
		return;
	}

	g_Device->CreateRenderTargetView(
		backBuffer,
		nullptr,
		&g_RenderTargetView
	);

	backBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_RenderTargetView != nullptr)
	{
		g_RenderTargetView->Release();
		g_RenderTargetView = nullptr;
	}
}

bool CreateDevice(HWND window)
{
	DXGI_SWAP_CHAIN_DESC swapChainDescription{};
	swapChainDescription.BufferCount = 2;
	swapChainDescription.BufferDesc.Format =
		DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDescription.BufferUsage =
		DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDescription.OutputWindow = window;
	swapChainDescription.SampleDesc.Count = 1;
	swapChainDescription.Windowed = TRUE;
	swapChainDescription.SwapEffect =
		DXGI_SWAP_EFFECT_DISCARD;

	constexpr D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	D3D_FEATURE_LEVEL selectedFeatureLevel{};

	const HRESULT result =
		D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			featureLevels,
			static_cast<UINT>(std::size(featureLevels)),
			D3D11_SDK_VERSION,
			&swapChainDescription,
			&g_SwapChain,
			&g_Device,
			&selectedFeatureLevel,
			&g_DeviceContext
		);

	if (FAILED(result))
	{
		return false;
	}

	CreateRenderTarget();
	return true;
}

void CleanupDevice()
{
	CleanupRenderTarget();

	if (g_SwapChain != nullptr)
	{
		g_SwapChain->Release();
		g_SwapChain = nullptr;
	}

	if (g_DeviceContext != nullptr)
	{
		g_DeviceContext->Release();
		g_DeviceContext = nullptr;
	}

	if (g_Device != nullptr)
	{
		g_Device->Release();
		g_Device = nullptr;
	}
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
);

LRESULT CALLBACK WindowProcedure(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
) {
	if (ImGui_ImplWin32_WndProcHandler(
		window,
		message,
		wParam,
		lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_SIZE:
		if (g_Device != nullptr &&
			wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();

			g_SwapChain->ResizeBuffers(
				0,
				LOWORD(lParam),
				HIWORD(lParam),
				DXGI_FORMAT_UNKNOWN,
				0
			);

			CreateRenderTarget();
		}

		return 0;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_KEYMENU)
		{
			return 0;
		}

		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(
		window,
		message,
		wParam,
		lParam
	);
}

bool UI::Init()
{
	m_HInstance = GetModuleHandleW(nullptr);

	if (m_HInstance == nullptr)
	{
		return false;
	}

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = m_HInstance;
	windowClass.lpszClassName = m_WindowClassName;

	RegisterClassExW(&windowClass);

	m_Window = CreateWindowExW(
		0,
		m_WindowClassName,
		L"Coyote Controller",
		WS_OVERLAPPEDWINDOW,
		100,
		100,
		900,
		600,
		nullptr,
		nullptr,
		m_HInstance,
		nullptr
	);

	if (m_Window == nullptr)
	{
		UnregisterClassW(m_WindowClassName, m_HInstance);
		return 1;
	}

	if (!CreateDevice(m_Window))
	{
		DestroyWindow(m_Window);
		UnregisterClassW(m_WindowClassName, m_HInstance);
		return 1;
	}

	ShowWindow(m_Window, SW_SHOWDEFAULT);
	UpdateWindow(m_Window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(m_Window);
	ImGui_ImplDX11_Init(
		g_Device,
		g_DeviceContext
	);

	bool running = true;
	float rumbleStrength = 0.5f;

	return true;
}

bool UI::Update(
	CoyoteBLEBackend& toyBackend,
	SignalBuffer& signalBuffer,
	TimeDuration historySpan
) {
	MSG message{};

	while (PeekMessageW(
		&message,
		nullptr,
		0,
		0,
		PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);

		if (message.message == WM_QUIT)
		{
			return false;
		}
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	// ==================== Begin Main UI ====================
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
			ImGui::Text(
				"Virtual controller: %s",
				m_VirtualControllerConnected ? "Connected" : "Disconnected"
			);

			ImGui::SameLine();

			bool toyConnected = toyBackend.IsPulseUnitConnected();
			if (toyConnected)
			{
				ImGui::Text("| Toy: Connected");
			}
			else
			{
				ImGui::Text("| Toy: Disconnected");
			}

			ImGui::SameLine();

			ImGui::SetCursorPosX(
				ImGui::GetWindowContentRegionMax().x -
				90.0f
			);
			const char* itemID = "Device List Popup";
			if (ButtonRAligned("Device List", 90.0f)) {
				ImGui::OpenPopup(itemID);
				toyBackend.StartScan();
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
						if (toyBackend.IsPawPrintConnected()) {
							ImGui::Text("PawPrint Button");
						}
					}
					ImGui::TableNextColumn();
					{
						const std::unordered_map<std::uint64_t, BLEAdvertisementInfo> advertisements =
							toyBackend.GetAdvertisements();
						if (advertisements.empty())
						{
							TextCentered("No bluetooth devices found", true);
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
									TextRAligned("Connected");
								}
								else if (p.second.connectionState == BLEConnectionState::Connecting)
								{
									ImGui::BeginDisabled();
									ButtonRAligned(std::format("Connecting...##{}", p.first).c_str(), 0.0f);
									ImGui::EndDisabled();
								}
								else if (ButtonRAligned(std::format("Connect##{}", p.first).c_str(), 0.0f))
								{
									toyBackend.ConnectAsync(p.first);
								}
							}
						}
					}

					ImGui::EndTable();
				}

				ImGui::EndPopup();
			}
			else {
				toyBackend.StopScan();
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
				if (toyBackend.IsSafetyOn())
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.52f, 0.52f, 0.52f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.62f, 0.31f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.49f, 0.19f, 1.0f));

					if (ImGui::Button("RESUME", ImVec2(-1.0f, 70.0f)))
					{
						toyBackend.SetSafety(false);
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
						toyBackend.SetSafety(true);
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
				TextCentered(textContent);
				if (ImGui::Button("-", ImVec2(-1.0f, 0.0f)))
				{
					strength = max(0.0f, strength - 1.0f);
				}

				signalBuffer.SetStrengthAmp(strength);

				ImGui::Separator();
				// ===== Disconnect =====
				if (ImGui::Button("Disconnect", ImVec2(-1.0f, 0.0f)))
				{
					toyBackend.DisconnectPulseUnit();
				}
			}
			else
			{
				TextCentered("No Toy Connected", true);
			}

			ImGui::EndChild();

			ImGui::SameLine();
			// ========== Signals ==========
			ImGui::BeginChild(
				"Visualization",
				ImVec2(0.0f, 0.0f)
			);

			const size_t numPlotPoints = static_cast<size_t>(historySpan / HISTORY_STRIDE);
			std::vector<float> leftChSignals(numPlotPoints);
			std::vector<float> rightChSignals(numPlotPoints);

			SignalBufferSnapshot signalBufferSnapshot = signalBuffer.GetSnapshot();
			if (signalBufferSnapshot->size() > 0) {
				TimeStamp startTime = std::chrono::steady_clock::now() - historySpan;

				for (size_t i = signalBufferSnapshot->size() - 1; i > 0; i--)
				{
					if ((*signalBufferSnapshot)[i].time < startTime)
					{
						break;
					}

					TimeStamp t = (*signalBufferSnapshot)[i - 1].time;
					if (t < startTime)
					{
						t = startTime;
					}
					for (; t < (*signalBufferSnapshot)[i].time; t += HISTORY_STRIDE)
					{
						size_t index = static_cast<size_t>((t - startTime) / HISTORY_STRIDE);
						if (index < numPlotPoints)
						{
							leftChSignals[index] = (*signalBufferSnapshot)[i - 1].left;
							rightChSignals[index] = (*signalBufferSnapshot)[i - 1].right;
						}
					}
				}
				RumbleSignal lastSignal = (*signalBufferSnapshot).back();
				for (
					TimeStamp t = lastSignal.time;
					t < std::chrono::steady_clock::now();
					t += HISTORY_STRIDE
					) {
					size_t index = static_cast<size_t>((t - startTime) / HISTORY_STRIDE);
					if (index < numPlotPoints)
					{
						leftChSignals[index] = lastSignal.left;
						rightChSignals[index] = lastSignal.right;
					}
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
	// ==================== End Main UI ====================
	ImGui::Render();

	constexpr float clearColor[4] =
	{
		0.08f,
		0.08f,
		0.08f,
		1.0f
	};

	g_DeviceContext->OMSetRenderTargets(
		1,
		&g_RenderTargetView,
		nullptr
	);

	g_DeviceContext->ClearRenderTargetView(
		g_RenderTargetView,
		clearColor
	);

	ImGui_ImplDX11_RenderDrawData(
		ImGui::GetDrawData()
	);

	g_SwapChain->Present(1, 0);

	return true;
}

void UI::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	ImPlot::DestroyContext();

	CleanupDevice();

	if (m_Window != nullptr) {
		DestroyWindow(m_Window);
	}
	if (m_HInstance != nullptr) {
		UnregisterClassW(m_WindowClassName, m_HInstance);
	}
}
