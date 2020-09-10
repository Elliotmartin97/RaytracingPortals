#pragma once
#include "stdafx.h"
#include "RayTracingHlslCompat.h"

namespace GlobalRootSignatureParams {
	enum Value {
		OutputViewSlot = 0,
		AccelerationStructureSlot,
		SceneConstantSlot,
		Count
	};
}

namespace LocalRootSignatureParams {
	enum Value {
		CubeConstantSlot = 0,
		PortalSlot,
		VertexBuffersSlot,
		Count
	};
}

union AlignedSceneConstantBuffer
{
	SceneConstantBuffer constants;
	uint8_t alignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};

struct RootArguments
{
	ObjectConstantBuffer constant_buffer;
	PortalSlot portal_slot;
	UINT64 gpu_handle;
};

class Scene;
class Portal;

class Raytracer
{
public:
	Raytracer(UINT heap_index);
	~Raytracer();
	void CreateRootSignatures(DX::DeviceResources* device_resources);
	void SerializeAndCreateRaytracingRootSignature(DX::DeviceResources* device_resources, D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
	void CreateRaytracingInterfaces(DX::DeviceResources* device_resources);
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
	void CreateRaytracingPipelineStateObject();
	void CreateRaytracingOutputResource(DX::DeviceResources* device_resources, UINT64 width, UINT64 height);
	void CreateDescriptorHeap(DX::DeviceResources* device_resources, int buffer_count);
	void CreateConstantBuffers(DX::DeviceResources* device_resources);
	void DoRaytracing(DX::DeviceResources* device_resourcecs, UINT width, UINT height, ComPtr<ID3D12Resource> TLAS);
	void BuildShaderTables(DX::DeviceResources* device_resources, std::vector<Portal> portal_origins);
	void ReleaseRaytracerResources();
	void CopyRaytracingOutputToBackbuffer(DX::DeviceResources* device_resources);
	//void BuildGeometryBuffers(DX::DeviceResources* device_resources, Raytracer* raytracer, Scene *scene);
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
	UINT CreateBufferSRV(DX::DeviceResources* device_resources, D3DBuffer* buffer, UINT numElements, UINT elementSize);
	ComPtr<ID3D12Device5> GetDXRDevice() { return dxr_device; }
	ComPtr<ID3D12GraphicsCommandList5> GetCMDList() { return dxr_commandList; }
	ComPtr<ID3D12StateObject> GetStateObject() { return dxr_state_object; }
	ObjectConstantBuffer& GetCubeCB() { return object_constant_buffer; }
	SceneConstantBuffer* GetSceneCB() { return scene_constant_buffers; }
	UINT GetFrameCount() { return FRAME_COUNT; }
	std::vector<D3DBuffer> GetIndexBuffers() { return index_buffer; }
	std::vector<D3DBuffer> GetVertexBuffers() { return vertex_buffer; }
	D3DBuffer* GetIndexBufferByIndex(int idx) { return &index_buffer[idx]; }
	D3DBuffer* GetVertexBufferByIndex(int idx) { return &vertex_buffer[idx]; }
	void AddBufferSlot() { index_buffer.resize(index_buffer.size() + 1); vertex_buffer.resize(vertex_buffer.size() + 1); }
	ComPtr<ID3D12Resource> GetRaytracingOutput() { return raytracing_output; }
	UINT GetShaderHitGroupStride() { return hit_group_stride_in_bytes; }
private:

	static const UINT FRAME_COUNT = 3;
	ComPtr<ID3D12Device5> dxr_device;
	ComPtr<ID3D12GraphicsCommandList5> dxr_commandList;
	ComPtr<ID3D12StateObject> dxr_state_object;
	ComPtr<ID3D12RootSignature> raytracing_global_root_signature;
	ComPtr<ID3D12RootSignature> raytracing_local_root_signature;
	ComPtr<ID3D12DescriptorHeap> descriptor_heap;
	ComPtr<ID3D12Resource> raytracing_output;
	D3D12_GPU_DESCRIPTOR_HANDLE raytracing_output_resource_UAV_gpu_descriptor;
	UINT descriptor_heap_index;
	UINT descriptors_allocated;
	UINT descriptor_size;
	static const wchar_t* hit_group_name[];
	static const wchar_t* raygen_shader_name;
	static const wchar_t* closest_hit_shader_names[];
	static const wchar_t* miss_shader_name;
	ComPtr<ID3D12Resource> miss_shader_table;
	ComPtr<ID3D12Resource> hit_group_shader_table;
	ComPtr<ID3D12Resource> ray_gen_shader_table;
	std::vector<D3DBuffer> index_buffer;
	std::vector<D3DBuffer> vertex_buffer;
	SceneConstantBuffer scene_constant_buffers[FRAME_COUNT];
	ObjectConstantBuffer object_constant_buffer;
	PortalSlot portal_constant_buffer;
	AlignedSceneConstantBuffer* mapped_constant_data;
	ComPtr<ID3D12Resource> per_frame_constants;
	UINT hit_group_stride_in_bytes = 0;
};