#include "stdafx.h"
#include "System.h"
#include "WindowsApplication.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE, LPSTR, int cmd_show)
{
    System system(1280, 720, L"D3D12 RaytracingPortals");
    return WindowsApplication::Run(&system, h_instance, cmd_show);
    //D3D12RaytracingSimpleLighting sample(1280, 720, L"D3D12 Raytracing - Simple Lighting");
    //return Win32Application::Run(&sample, hInstance, nCmdShow);
}