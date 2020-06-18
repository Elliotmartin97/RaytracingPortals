#pragma once

#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"

//forward declerations
class Model;
class Raytracer;
class AccelerationStructure;

class D3D12RaytracingSimpleLighting : public DXSample
{
public:
    D3D12RaytracingSimpleLighting(UINT width, UINT height, std::wstring name);

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
    virtual void OnDestroy();
    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

private:
    static const UINT FrameCount = 3;

    // We'll allocate space for several of these and they will need to be padded for alignment.
    static_assert(sizeof(SceneConstantBuffer) < D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Checking the size here.");

    // Geometry
    struct D3DBuffer
    {
        ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
    };

    UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;

    
    // Application state
    StepTimer m_timer;
    float m_curRotationAngleRad;
    XMVECTOR m_eye;
    XMVECTOR m_at;
    XMVECTOR m_up;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs;
    std::vector<int> index_buffer_offsets;
    std::vector<int> vertex_buffer_offsets;
    std::vector<Index> scene_indicies;
    std::vector<Vertex> scene_vertices;
    void UpdateCameraMatrices();
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
    Model* cube_model;
    Model* model2;
};
