#include "stdafx.h"
#include "WindowsApplication.h"
#include "DXHelper.h"

HWND WindowsApplication::hwnd = nullptr;
bool WindowsApplication::fullscreen_mode = false;
RECT WindowsApplication::window_rect;
using Microsoft::WRL::ComPtr;

int WindowsApplication::Run(System* system, HINSTANCE h_instance, int cmd_show)
{
    try
    {
        // Parse the command line parameters
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        system->ParseCLA(argv, argc);
        LocalFree(argv);

        // Initialize the window class.
        WNDCLASSEX windowClass = { 0 };
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = WindowProccess;
        windowClass.hInstance = h_instance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = L"DXSampleClass";
        RegisterClassEx(&windowClass);

        RECT windowRect = { 0, 0, static_cast<LONG>(system->GetWidth()), static_cast<LONG>(system->GetHeight()) };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        // Create the window and store a handle to it.
        hwnd = CreateWindow(
            windowClass.lpszClassName,
            system->GetTitle(),
            window_style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            nullptr,        // We have no parent window.
            nullptr,        // We aren't using menus.
            h_instance,
            system);

        // Initialize the sample. OnInit is defined in each child-implementation of DXSample.
        system->Init();

        ShowWindow(hwnd, cmd_show);

        // Main sample loop.
        MSG msg = {};
        while (msg.message != WM_QUIT)
        {
            // Process any messages in the queue.
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        system->Destroy();

        // Return this part of the WM_QUIT message to Windows.
        return static_cast<char>(msg.wParam);
    }
    catch (std::exception& e)
    {
        OutputDebugString(L"Application hit a problem: ");
        OutputDebugStringA(e.what());
        OutputDebugString(L"\nTerminating.\n");

        system->Destroy();
        return EXIT_FAILURE;
    }
}

void WindowsApplication::FullScreen(IDXGISwapChain* swap_chain)
{
    if (fullscreen_mode)
    {
        // Restore the window's attributes and size.
        SetWindowLong(hwnd, GWL_STYLE, window_style);

        SetWindowPos(
            hwnd,
            HWND_NOTOPMOST,
            window_rect.left,
            window_rect.top,
            window_rect.right - window_rect.left,
            window_rect.bottom - window_rect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hwnd, SW_NORMAL);
    }
    else
    {
        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(hwnd, &window_rect);

        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(hwnd, GWL_STYLE, window_style & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT fullscreenWindowRect;
        try
        {
            if (swap_chain)
            {
                // Get the settings of the display on which the app's window is currently displayed
                ComPtr<IDXGIOutput> pOutput;
                ThrowIfFailed(swap_chain->GetContainingOutput(&pOutput));
                DXGI_OUTPUT_DESC Desc;
                ThrowIfFailed(pOutput->GetDesc(&Desc));
                fullscreenWindowRect = Desc.DesktopCoordinates;
            }
            else
            {
                // Fallback to EnumDisplaySettings implementation
                throw HrException(S_FALSE);
            }
        }
        catch (HrException& e)
        {
            UNREFERENCED_PARAMETER(e);

            // Get the settings of the primary display
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            fullscreenWindowRect = {
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
                devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
            };
        }

        SetWindowPos(
            hwnd,
            HWND_TOPMOST,
            fullscreenWindowRect.left,
            fullscreenWindowRect.top,
            fullscreenWindowRect.right,
            fullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);


        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    fullscreen_mode = !fullscreen_mode;
}

void WindowsApplication::WindowZOrder(bool top)
{
    RECT rect;
    GetWindowRect(hwnd, &rect);
    SetWindowPos(
        hwnd,
        (top) ? HWND_TOPMOST : HWND_NOTOPMOST,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

LRESULT CALLBACK WindowsApplication::WindowProccess(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{

}

