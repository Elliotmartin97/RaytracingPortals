#pragma once
#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"

//forward declerations
class Model;
class Raytracer;
class AccelerationStructure;
class Scene;
class Camera;

class Graphics : public DXSample
{
public:
    Graphics(UINT width, UINT height, std::wstring name);

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
    virtual void OnDestroy();
    void OnMouseMove(UINT x, UINT y) override;
    void OnKeyDown(UINT8 key) override;
    void SetWindowCenterPositions(int x, int y) override;
    virtual IDXGISwapChain* GetSwapchain() { return device_resources->GetSwapChain(); }

private:
    static const UINT FRAME_COUNT = 3;
    static_assert(sizeof(SceneConstantBuffer) < D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Size here.");

    struct D3DBuffer
    {
        ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle;
    };

    UINT raytracing_output_resource_UAV_descriptor_heap_index;
    StepTimer timer;
    void InitializeScene();
    void RecreateD3D();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();
    void ReleaseWindowSizeDependentResources();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void CalculateFrameStats();
    Raytracer* raytracer;
    AccelerationStructure* acceleration_structure;
    Scene* scene;
    Camera* camera;
    int window_center_x = 0;
    int window_center_y = 0;
};
