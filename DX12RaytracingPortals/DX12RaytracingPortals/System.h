#pragma once

#include "Windows.h"
#include "DeviceResources.h"
#include "DXHelper.h"

class System : public DX::IDeviceNotify
{
public:
	System(UINT width_param, UINT height_param, std::wstring name);
	virtual ~System();

	virtual void Init() = 0;
	virtual void OnUpdate() = 0;
	virtual void Render() = 0;
	virtual void WindowSizeChanged(UINT width, UINT height, bool minimized) = 0;
	virtual void Destroy() = 0;

	virtual void KeyDown(UINT8) {}
	virtual void KeyUP(UINT8) {}
	virtual void WindowMoved(int, int) {}
	virtual void MouseMoved(UINT, UINT) {}
	virtual void LeftButtonDown(UINT, UINT) {}
	virtual void LeftButtonUp(UINT, UINT) {}
	virtual void DisplayChanged() {}

	virtual void ParseCLA(_In_reads_(argc) WCHAR* argv[], int argc);

	UINT GetWidth() const { return width; }
	UINT GetHeight() const { return height; }
	const WCHAR* GetTitle() const { return title.c_str(); }
	RECT GetWindowBounds() const { return window_bounds; }
	virtual IDXGISwapChain* GetSwapChain() { return nullptr; }
	DX::DeviceResources* GetDeviceResources() const { return device_resources.get(); }

	void UpdateSizeChange(UINT width, UINT height);
	void SetWindowBounds(int left, int top, int right, int bottom);
	std::wstring GetAssetFullPath(LPCWSTR name);
protected:
	void SetCustomWindowText(LPCWSTR text);

	UINT width;
	UINT height;
	float aspect_ratio;

	RECT window_bounds;
	bool enable_UI;
	UINT adapter_ID_override;
	std::unique_ptr<DX::DeviceResources> device_resources;
private:
	std::wstring asset_path;
	std::wstring title;

};