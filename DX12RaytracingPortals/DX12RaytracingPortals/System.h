#pragma once

#include "WindowsApplication.h"
#include "DeviceResources.h"
#include "DXHelper.h"
#include "HLSLRaytracingShader.h"

class Raytracer;

class System : public DX::IDeviceNotify
{
public:
	System(UINT width_param, UINT height_param, std::wstring name);
	~System();

	void WindowSizeChanged(UINT width, UINT height, bool minimized);
	void Init();
	void Destroy();
	void Update();

	void KeyDown(UINT8) {}
	void KeyUp(UINT8) {}
	void WindowMoved(int, int) {}
	void MouseMoved(UINT, UINT) {}
	void LeftButtonDown(UINT, UINT) {}
	void LeftButtonUp(UINT, UINT) {}
	void DisplayChanged() {}
	inline bool CheckRaytracingSupported(IDXGIAdapter1* adapter);

	void ParseCLA(_In_reads_(argc) WCHAR* argv[], int argc);
	virtual void OnDeviceLost() override;
	virtual void OnDeviceRestored() override;

	UINT GetWidth() const { return width; }
	UINT GetHeight() const { return height; }
	const WCHAR* GetTitle() const { return title.c_str(); }
	RECT GetWindowBounds() const { return window_bounds; }
	IDXGISwapChain* GetSwapChain() { return device_resources->GetSwapChain(); }
	DX::DeviceResources* GetDeviceResources() const { return device_resources.get(); }

	void UpdateSizeChange(UINT width, UINT height);
	void SetWindowBounds(int left, int top, int right, int bottom);
	std::wstring GetAssetFullPath(LPCWSTR name);

	UINT GetWidth() { return width; }
	UINT GetHeight() { return height; }
	void SetCustomWindowText(LPCWSTR text);
protected:
	static const UINT frame_count = 3;

	//constant buffers
	static_assert(sizeof(SceneConstantBuffer) < D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Checking the size here.");
	union AlignedSceneConstantBuffer
	{
		SceneConstantBuffer constants;
		uint8_t alignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
	};
	AlignedSceneConstantBuffer* mapped_constant_data;
	ComPtr<ID3D12Resource>       per_frame_constants;

	UINT width;
	UINT height;
	float aspect_ratio;

	RECT window_bounds;
	bool enable_UI;
	UINT adapter_ID_override;
	std::unique_ptr<DX::DeviceResources> device_resources;
	Raytracer* raytracer;
private:
	std::wstring asset_path;
	std::wstring title;

};