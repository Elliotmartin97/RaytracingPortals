#pragma once
#include "stdafx.h"
#include "WindowsApplication.h"

namespace GlobalRootSignatureParams {
    enum Value {
        OutputViewSlot = 0,
        AccelerationStructureSlot,
        SceneConstantSlot,
        VertexBuffersSlot,
        Count
    };
}

namespace LocalRootSignatureParams {
    enum Value {
        CubeConstantSlot = 0,
        Count
    };
}

class System;

static const wchar_t* hit_group_name;
static const wchar_t* ray_shader_name;
static const wchar_t* closest_hit_name;
static const wchar_t* miss_shader_name;

class Raytracer
{
public:
    Raytracer(DX::DeviceResources& resources, int f_count);
    ~Raytracer();
    void InitRaytracer();
	void Render();
    void Destroy();
    void DeviceLost();
    void DeviceRestored();
    void OnSizeChanged(UINT width, UINT height, bool minimized);
private:

    void InitScene();
    void RecreateD3D();
    void RaytraceScene();
    void CreateConstantBuffers();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();
    void ReleaseWindowSizeDependentResources();
    void CreateRaytracingInterfaces();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* root_signature);
    void CreateRootSignatures();
    void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracing_pipeline);
    void CreateRaytracingPipelineStateObject();
    void CreateDescriptorHeap();
    void CreateRaytracingOutputResource();
    void BuildGeometry();
    void BuildAccelerationStructures();
    void BuildShaderTables();
    void CopyRaytracingOutputToBackbuffer();
    void CalculateFrameStats();
    UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_descriptor, UINT descriptor_index = UINT_MAX);
    UINT CreateBufferSRV(D3DBuffer* buffer, UINT num_elements, UINT size);


    union AlignedSceneConstantBuffer
    {
        SceneConstantBuffer constants;
        uint8_t alignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
    };
    AlignedSceneConstantBuffer* mapped_constants;
    ComPtr<ID3D12Resource>       per_frame_constant;

    ComPtr<ID3D12Device5> dxr_device;
    ComPtr<ID3D12GraphicsCommandList5> dxr_command_list;
    ComPtr<ID3D12StateObject> dxr_state_object;

    ComPtr<ID3D12RootSignature> raytrace_global_root_signature;
    ComPtr<ID3D12RootSignature> raytrace_local_root_signature;

    ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
    ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    UINT descriptors_allocated;
    UINT descriptor_size;

    std::vector<SceneConstantBuffer> m_sceneCB;
    CubeConstantBuffer m_cubeCB;

    D3DBuffer index_buffer;
    D3DBuffer vertex_buffer;

    ComPtr<ID3D12Resource> raytracing_output;
    D3D12_GPU_DESCRIPTOR_HANDLE raytracing_gpu_descriptor;
    UINT raytracing_descriptor_index;

    ComPtr<ID3D12Resource> miss_shader_table;
    ComPtr<ID3D12Resource> hit_group_table;
    ComPtr<ID3D12Resource> ray_shader_table;

    //steptimer timer

    float rotation_angle_rad;
    XMVECTOR eye;
    XMVECTOR at;
    XMVECTOR up;

    DX::DeviceResources* device_resources;
    System* system;
};