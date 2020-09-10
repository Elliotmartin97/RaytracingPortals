#include "stdafx.h"
#include "Graphics.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    Graphics sample(1280, 720, L"Raytracing With Portals");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
