#pragma once

#include <unordered_map>
#include <Windows.h>

#include <d3d11.h>
#include <tchar.h>

#include <iostream>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "implot.h"

#include "ToyBackend/CoyoteBLEBackend.h"
#include "SignalBuffer.h"

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

	static bool BeginFrame();
	static void EndFrame();

	static void TextCentered(const char* textContent, bool yCentered = false);
	static void TextRAligned(const char* textContent);
	static bool ButtonRAligned(const char* label, float width);

	static void Shutdown();

private:
	static HINSTANCE m_HInstance;
	static const wchar_t* m_WindowClassName;
	static HWND m_Window;
};

