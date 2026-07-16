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
bool UI::m_ToyConnected = false;

constexpr TimeDuration HISTORY_STRIDE = std::chrono::milliseconds(50);
constexpr ImVec4 WAVEFORM_COLOR_L(0.26f, 0.53f, 0.96f, 0.9f);
constexpr ImVec4 WAVEFORM_COLOR_R(0.96f, 0.69f, 0.26f, 0.9f);

void CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;

    g_SwapChain->GetBuffer(
        0,
        IID_PPV_ARGS(&backBuffer)
    );

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

	if (toyBackend.IsConnected())
    {
        m_ToyConnected = true;
    }
    else if (m_ToyConnected)
    {
		m_ToyConnected = false;
		toyBackend.StartScan();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    // ==================== Begin Main UI ====================
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("Main", nullptr, flags);
        {
			// ========== Connectivity ==========
            ImGui::Text(
                "Virtual controller: %s",
                m_VirtualControllerConnected ? "Connected" : "Disconnected"
            );

            ImGui::SameLine();

            if (m_ToyConnected)
            {
                ImGui::Text("| Toy: Connected  (%d%%)");
            }
			else
			{
				ImGui::Text("| Toy: Disconnected");
			}

            ImGui::Separator();

            // ========== Side Panel ==========
            ImGui::BeginChild(
                "Controls",
                ImVec2(320.0f, 0.0f),
                ImGuiChildFlags_Borders
            );

            if (m_ToyConnected)
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

                const float buttonWidth = 40.0f;
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                char label[15];
                std::snprintf(label, 15, "Strength: %.0f", strength);
                const float textWidth = ImGui::CalcTextSize(label).x;
                const float rowWidth =
                    buttonWidth +
                    spacing +
                    textWidth +
                    spacing +
                    buttonWidth;
                const float availableWidth = ImGui::GetContentRegionAvail().x;
                const float offset = max(0.0f, (availableWidth - rowWidth) * 0.5f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

                if (ImGui::Button("-", ImVec2(buttonWidth, 0.0f)))
                {
                    strength = max(0.0f, strength - 1.0f);
                }
                ImGui::SameLine();
				ImGui::Text("Strength: %.0f", strength);
                ImGui::SameLine();
                if (ImGui::Button("+", ImVec2(buttonWidth, 0.0f)))
                {
                    strength += 1.0f;
                }

                signalBuffer.SetStrengthAmp(strength);

                ImGui::Separator();
                // ===== Disconnect =====
                if (ImGui::Button("Disconnect", ImVec2(-1.0f, 0.0f)))
                {
                    toyBackend.Disconnect();
                }
            }
            else
            {
				const std::unordered_map<std::uint64_t, BLEAdvertisementInfo> advertisements =
					toyBackend.GetAdvertisements();
                if (advertisements.empty())
                {
                    ImGui::TextUnformatted("No bluetooth devices found.");
                }
                else
                {
                    for (const std::pair<std::uint64_t, BLEAdvertisementInfo>& p : advertisements)
                    {
                        ImGui::Text("%s", p.second.name.c_str());
                        ImGui::Text("RSSI: %d dBm", p.second.rssi);

                        ImGui::SameLine();

                        if (p.second.connectionState == BLEConnectionState::Connected)
                        {
                            ImGui::TextUnformatted("Connected");
                        }
                        else if (p.second.connectionState == BLEConnectionState::Connecting)
                        {
                            ImGui::BeginDisabled();
                            ImGui::Button("Connecting...");
                            ImGui::EndDisabled();
                        }
                        else if (ImGui::Button("Connect"))
                        {
							toyBackend.ConnectAsync(p.first);
                        }
                    }
                }
            }

            ImGui::EndChild();

            ImGui::SameLine(); 

            // ========== Signals ==========
            ImGui::BeginChild(
                "Visualization",
                ImVec2(0.0f, 0.0f)
            );

            ImGui::ColorButton(
                "##LeftMotorColor",
                WAVEFORM_COLOR_L,
                ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoDragDrop,
                ImVec2(12.0f, 12.0f)
            );
            ImGui::SameLine();
            ImGui::TextUnformatted("Left motor");
            ImGui::SameLine();
            ImGui::ColorButton(
                "##RightMotorColor",
                WAVEFORM_COLOR_R,
                ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoDragDrop,
                ImVec2(12.0f, 12.0f)
            );
            ImGui::SameLine();
            ImGui::TextUnformatted("Right motor");

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

            ImGui::PushStyleColor(ImGuiCol_PlotLines, WAVEFORM_COLOR_L);
            ImGui::PlotLines(
                "##LSignals",
                leftChSignals.data(),
                static_cast<int>(numPlotPoints),
                0,              // offset
                nullptr,        // overlay text
                0.0f,           // minimum amplitude
                1.0f,           // maximum amplitude
                ImVec2(-1.0f, 120.0f)
            );
            ImGui::PushStyleColor(ImGuiCol_PlotLines, WAVEFORM_COLOR_R);
            ImGui::PlotLines(
                "##RSignals",
                rightChSignals.data(),
                static_cast<int>(numPlotPoints),
                0,              // offset
                nullptr,        // overlay text
                0.0f,           // minimum amplitude
                1.0f,           // maximum amplitude
                ImVec2(-1.0f, 120.0f)
            );

            ImGui::PopStyleColor(2);

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

    CleanupDevice();

    if (m_Window != nullptr) {
        DestroyWindow(m_Window);
    }
    if (m_HInstance != nullptr) {
        UnregisterClassW(m_WindowClassName, m_HInstance);
    }
}
