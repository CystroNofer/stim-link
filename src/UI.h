#pragma once

#include <Windows.h>

#include <d3d11.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

void CreateRenderTarget();

void CleanupRenderTarget();

bool CreateDevice(HWND window);

void CleanupDevice();

LRESULT CALLBACK WindowProcedure(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
);

class UI
{
public:
	static bool Init();

	static bool Update();

	static void Shutdown();

	static inline void UpdateAppState(
		bool virtualControllerConnected,
		bool coyoteConnected
	) {
		m_VirtualControllerConnected = virtualControllerConnected;
		m_CoyoteConnected = coyoteConnected;
	}

	static inline void UpdateWaveform(
		float leftChannel,
		float rightChannel
	) {

	}

private:
	static HINSTANCE m_HInstance;
	static const wchar_t* m_WindowClassName;
	static HWND m_Window;

	static bool m_VirtualControllerConnected;
	static bool m_CoyoteConnected;
};

