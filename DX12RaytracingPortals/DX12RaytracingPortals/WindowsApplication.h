#pragma once
#include "System.h"
#include "stdafx.h"
#include "d3dx12.h"

class System;

class WindowsApplication
{
public:
	static int Run(System* system, HINSTANCE h_instance, int cmd_show);
	static void FullScreen(IDXGISwapChain* output = nullptr);
	static void WindowZOrder(bool top);
	static HWND GetHwnd() { return hwnd; }
	static bool IsFullscreen() { return fullscreen_mode; }

protected:
	static LRESULT CALLBACK WindowProccess(HWND hwnd, UINT messsage, WPARAM w_param, LPARAM l_param);

private:
	static HWND hwnd;
	static bool fullscreen_mode;
	static const UINT window_style = WS_OVERLAPPEDWINDOW;
	static RECT window_rect;
};