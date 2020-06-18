#pragma once
#include "stdafx.h"
#include "RayTracingHlslCompat.h"

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

union AlignedSceneConstantBuffer
{
	SceneConstantBuffer constants;
	uint8_t alignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};

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
	void CreateDescriptorHeap(DX::DeviceResources* device_resources);
	void CreateConstantBuffers(DX::DeviceResources* device_resources);
	void DoRaytracing(DX::DeviceResources* device_resourcecs, UINT width, UINT height, ComPtr<ID3D12Resource> m_topLevelAccelerationStructure);
	void BuildShaderTables(DX::DeviceResources* device_resources);
	void ReleaseRaytracerResources();
	void CopyRaytracingOutputToBackbuffer(DX::DeviceResources* device_resources);
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
	UINT CreateBufferSRV(DX::DeviceResources* device_resources, D3DBuffer* buffer, UINT numElements, UINT elementSize);
	ComPtr<ID3D12Device5> GetDXRDevice() { return m_dxrDevice; }
	ComPtr<ID3D12GraphicsCommandList5> GetCMDList() { return m_dxrCommandList; }
	ComPtr<ID3D12StateObject> GetStateObject() { return m_dxrStateObject; }
	CubeConstantBuffer& GetCubeCB() { return m_cubeCB; }
	SceneConstantBuffer* GetSceneCB() { return m_sceneCB; }
	UINT GetFrameCount() { return FrameCount; }
	D3DBuffer* GetIndexBuffer() { return &m_indexBuffer; }
	D3DBuffer* GetVertexBuffer() { return &m_vertexBuffer; }
	ComPtr<ID3D12Resource> GetRaytracingOutput() { return m_raytracingOutput; }
private:

	static const UINT FrameCount = 3;
	ComPtr<ID3D12Device5> m_dxrDevice;
	ComPtr<ID3D12GraphicsCommandList5> m_dxrCommandList;
	ComPtr<ID3D12StateObject> m_dxrStateObject;
	ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature;
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	ComPtr<ID3D12Resource> m_raytracingOutput;
	D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
	UINT descriptor_heap_index;
	UINT m_descriptorsAllocated;
	UINT m_descriptorSize;
	static const wchar_t* c_hitGroupName;
	static const wchar_t* c_raygenShaderName;
	static const wchar_t* c_closestHitShaderName;
	static const wchar_t* c_missShaderName;
	ComPtr<ID3D12Resource> m_missShaderTable;
	ComPtr<ID3D12Resource> m_hitGroupShaderTable;
	ComPtr<ID3D12Resource> m_rayGenShaderTable;
	D3DBuffer m_indexBuffer;
	D3DBuffer m_vertexBuffer;
	SceneConstantBuffer m_sceneCB[FrameCount];
	CubeConstantBuffer m_cubeCB;
	AlignedSceneConstantBuffer* m_mappedConstantData;
	ComPtr<ID3D12Resource>       m_perFrameConstants;
};