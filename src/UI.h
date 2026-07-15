#pragma once

#include <Windows.h>

#include <d3d11.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "Waveform.h"

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

	static bool Update(
		const std::vector<RumbleSignal>& rumbleSignals,
		TimeDuration historySpan = std::chrono::milliseconds(5000)
	);

	static void Shutdown();

	static inline void UpdateVControllerState(
		bool connected
	) {
		m_VirtualControllerConnected = connected;
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
	static bool m_ToyConnected;
};

