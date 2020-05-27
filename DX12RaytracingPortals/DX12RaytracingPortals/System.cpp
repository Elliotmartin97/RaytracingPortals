#include "System.h"
#include "Windows.h"

System::System(UINT width_param, UINT height_param, std::wstring name)
{
	width = width_param;
	height = height_param;
	window_bounds = { 0,0,0,0 };
	title = name;
	aspect_ratio = 0.0f;
	enable_UI = true;
	adapter_ID_override = (UINT_MAX);

	WCHAR path[512];
	GetAssetsPath(path, _countof(path));
	asset_path = path;

	UpdateSizeChange(width, height);
	
}

System::~System() {}

void System::UpdateSizeChange(UINT w, UINT h)
{
	width = w;
	height = h;
	aspect_ratio = static_cast<float>(w) / static_cast<float>(h);
}

std::wstring System::GetAssetFullPath(LPCWSTR name)
{
	return asset_path + name;
}

void System::SetCustomWindowText(LPCWSTR text)
{
	std::wstring window_text = title + L":" + text;
	SetWindowText(Windows::Ge)
}