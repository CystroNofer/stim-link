#include "UI.h"

#include <iterator>

ID3D11Device* g_Device = nullptr;
ID3D11DeviceContext* g_DeviceContext = nullptr;
IDXGISwapChain* g_SwapChain = nullptr;
ID3D11RenderTargetView* g_RenderTargetView = nullptr;

HINSTANCE UI::m_HInstance = nullptr;
const wchar_t* UI::m_WindowClassName = L"CoyoteControllerWindow";
HWND UI::m_Window = nullptr;

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

	return true;
}

bool UI::BeginFrame()
{
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

	return true;
}

void UI::EndFrame() {
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
}

void UI::TextCentered(const char* textContent, bool yCentered) {
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

void UI::TextRAligned(const char* textContent) {
	ImGui::SetCursorPosX(
		ImGui::GetCursorPosX() +
		ImGui::GetContentRegionAvail().x -
		ImGui::CalcTextSize(textContent).x
	);
	ImGui::TextUnformatted(textContent);
}

bool UI::ButtonRAligned(const char* textContent, float width) {
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
