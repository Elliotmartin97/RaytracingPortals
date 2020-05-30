#include "System.h"
#include "WindowsApplication.h"
#include "DeviceResources.h"

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

void System::Init()
{
	device_resources = std::make_unique<DX::DeviceResources>(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_UNKNOWN,
		frame_count,
		D3D_FEATURE_LEVEL_11_0,
		DX::DeviceResources::c_RequireTearingSupport,
		adapter_ID_override
		);
	device_resources->RegisterDeviceNotify(this);
	device_resources->SetWindow(WindowsApplication::GetHwnd(), width, height);
	device_resources->InitializeDXGIAdapter();

	ThrowIfFalse(CheckRaytracingSupported(device_resources->GetAdapter()),
		L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

	device_resources->CreateDeviceResources();
	device_resources->CreateWindowSizeDependentResources();

	//Init raytracer
	
}

void System::Destroy()
{
	device_resources->WaitForGpu();
	OnDeviceLost();
}

void System::Update()
{

}
void System::UpdateSizeChange(UINT w, UINT h)
{
	width = w;
	height = h;
	aspect_ratio = static_cast<float>(w) / static_cast<float>(h);
}

void System::WindowSizeChanged(UINT width, UINT height, bool minimized)
{
	if (!device_resources->WindowSizeChanged(width, height, minimized))
	{
		return;
	}

	UpdateSizeChange(width, height);

	/*ReleaseWindowSizeDependentResources();
	CreateWindowSizeDependentResources();*/
}

std::wstring System::GetAssetFullPath(LPCWSTR name)
{
	return asset_path + name;
}

void System::SetCustomWindowText(LPCWSTR text)
{
	std::wstring window_text = title + L":" + text;
	SetWindowText(WindowsApplication::GetHwnd(), window_text.c_str());
}

void System::ParseCLA(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-disableUI", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/disableUI", wcslen(argv[i])) == 0)
		{
			enable_UI = false;
		}
		else if (_wcsnicmp(argv[i], L"-forceAdapter", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/forceAdapter", wcslen(argv[i])) == 0)
		{
			ThrowIfFalse(i + 1 < argc, L"Incorrect argument format passed in.");

			adapter_ID_override = _wtoi(argv[i + 1]);
			i++;
		}
	}
}

void System::SetWindowBounds(int left, int top, int right, int bottom)
{
	window_bounds.left = static_cast<LONG>(left);
	window_bounds.top = static_cast<LONG>(top);
	window_bounds.right = static_cast<LONG>(right);
	window_bounds.bottom = static_cast<LONG>(bottom);
}

void System::OnDeviceLost()
{
	
}

void System::OnDeviceRestored()
{

}

inline bool System::CheckRaytracingSupported(IDXGIAdapter1* adapter)
{
	ComPtr<ID3D12Device> testDevice;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	return SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
		&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}