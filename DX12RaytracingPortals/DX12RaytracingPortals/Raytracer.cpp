#include "stdafx.h"
#include "Raytracer.h"
#include "AccelerationStructure.h"
#include "DXRHelper.h"
#include "Raytracing.hlsl.h"


Raytracer::Raytracer(DX::DeviceResources& resources, int f_count)
{
	device_resources = &resources;
	m_sceneCB.resize(f_count);
	hit_group_name = L"MyHitGroup";
	ray_shader_name = L"MyRaygenShader";
	closest_hit_name = L"MyClosestHitShader";
	miss_shader_name = L"MyMissShader";
}

Raytracer::~Raytracer()
{
	
}

void Raytracer::InitRaytracer()
{
	
}

void Raytracer::InitScene()
{
	auto frame_index = device_resources->GetCurrentFrameIndex();

	m_cubeCB.albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	// Initialize the view and projection inverse matrices.
	eye = { 0.0f, 2.0f, -5.0f, 1.0f };
	at = { 0.0f, 0.0f, 0.0f, 1.0f };
	XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

	XMVECTOR direction = XMVector4Normalize(at - eye);
	up = XMVector3Normalize(XMVector3Cross(direction, right));

	// Rotate camera around Y axis.
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
	eye = XMVector3Transform(eye, rotate);
	up = XMVector3Transform(up, rotate);

	//UpdateCameraMatrices();

	XMFLOAT4 lightPosition;
	XMFLOAT4 lightAmbientColor;
	XMFLOAT4 lightDiffuseColor;

	lightPosition = XMFLOAT4(0.0f, 1.8f, -3.0f, 0.0f);
	m_sceneCB[frame_index].light_pos = XMLoadFloat4(&lightPosition);
	lightAmbientColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_sceneCB[frame_index].light_ambient_color = XMLoadFloat4(&lightAmbientColor);
	lightDiffuseColor = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
	m_sceneCB[frame_index].light_diffuse_color = XMLoadFloat4(&lightDiffuseColor);

	for (auto& SceneCB : m_sceneCB)
	{
		SceneCB = m_sceneCB[frame_index];
	}
}

void Raytracer::CreateConstantBuffers()
{
	auto device = device_resources->GetD3DDevice();
	auto frameCount = device_resources->GetBackBufferCount();
    const D3D12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    size_t cbSize = frameCount * sizeof(AlignedSceneConstantBuffer);
    const D3D12_RESOURCE_DESC constant_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &constant_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&per_frame_constant)));

    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(per_frame_constant->Map(0, nullptr, reinterpret_cast<void**>(&mapped_constants)));
}

void Raytracer::CreateDeviceDependentResources()
{

	CreateRaytracingInterfaces();

	// Create root signatures for the shaders.
	CreateRootSignatures();

	// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
	CreateRaytracingPipelineStateObject();

	// Create a heap for descriptors.
	CreateDescriptorHeap();

	// Build geometry to be used in the sample.
	BuildGeometry();

	// Build raytracing acceleration structures from the generated geometry.
	BuildAccelerationStructures();

	// Create constant buffers for the geometry and the scene.
	CreateConstantBuffers();

	// Build shader tables, which define shaders and their local root arguments.
	BuildShaderTables();

	// Create an output 2D texture to store the raytracing result to.
	CreateRaytracingOutputResource();
}

void Raytracer::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* root_sig)
{
	auto device = device_resources->GetD3DDevice();
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
	ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*root_sig))));
}

void Raytracer::CreateRootSignatures()
{
	auto device = device_resources->GetD3DDevice();

	CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

	CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
	rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
	rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
	rootParameters[GlobalRootSignatureParams::SceneConstantSlot].InitAsConstantBufferView(0);
	rootParameters[GlobalRootSignatureParams::VertexBuffersSlot].InitAsDescriptorTable(1, &ranges[1]);
	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &raytrace_global_root_signature);

	CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
	rootParameters[LocalRootSignatureParams::CubeConstantSlot].InitAsConstants(SizeOfInUint32(m_cubeCB), 1);
	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &raytrace_local_root_signature);
}

void Raytracer::CreateRaytracingInterfaces()
{
	auto device = device_resources->GetD3DDevice();
	auto commandList = device_resources->GetCommandList();

	ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxr_device)), L"Couldn't get DirectX Raytracing interface for the device.\n");
	ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxr_command_list)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

void Raytracer::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracing_pipeline)
{
	auto localRootSignature = raytracing_pipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSignature->SetRootSignature(raytrace_local_root_signature.Get());
	// Define explicit shader association for the local root signature. 
	{
		auto rootSignatureAssociation = raytracing_pipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation->AddExport(hit_group_name);
	}
}

void Raytracer::CreateRaytracingPipelineStateObject()
{
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
	auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
	lib->SetDXILLibrary(&libdxil);
	// Define which shader exports to surface from the library.
	// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
	// In this sample, this could be ommited for convenience since the sample uses all shaders in the library. 
	{
		lib->DefineExport(ray_shader_name);
		lib->DefineExport(closest_hit_name);
		lib->DefineExport(miss_shader_name);
	}

	// Triangle hit group
	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
	// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
	auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hitGroup->SetClosestHitShaderImport(closest_hit_name);
	hitGroup->SetHitGroupExport(hit_group_name);
	hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4);    // float4 pixelColor
	UINT attributeSize = sizeof(XMFLOAT2);  // float2 barycentrics
	shaderConfig->Config(payloadSize, attributeSize);

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	CreateLocalRootSignatureSubobjects(&raytracingPipeline);

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(raytrace_global_root_signature.Get());

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// PERFOMANCE TIP: Set max recursion depth as low as needed 
	// as drivers may apply optimization strategies for low recursion depths.
	UINT maxRecursionDepth = 1; // ~ primary rays only. 
	pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
	PrintStateObjectDesc(raytracingPipeline);
#endif

	// Create the state object.
	ThrowIfFailed(dxr_device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&dxr_state_object)), L"Couldn't create DirectX Raytracing state object.\n");
}

void Raytracer::CreateRaytracingOutputResource()
{
	auto device = device_resources->GetD3DDevice();
	auto back_buffer_format = device_resources->GetBackBufferFormat();
	auto uav_desc = CD3DX12_RESOURCE_DESC::Tex2D(back_buffer_format, system->GetWidth(), system->GetHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	auto default_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(device->CreateCommittedResource(
		&default_heap_properties, D3D12_HEAP_FLAG_NONE, &uav_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracing_output)));
	NAME_D3D12_OBJECT(raytracing_output);

	D3D12_CPU_DESCRIPTOR_HANDLE uav_desc_handle;
	raytracing_descriptor_index = AllocateDescriptor(&uav_desc_handle, raytracing_descriptor_index);
	D3D12_UNORDERED_ACCESS_VIEW_DESC unordered_desc = {};
	unordered_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(raytracing_output.Get(), nullptr, &unordered_desc, uav_desc_handle);
	raytracing_gpu_descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_heap->GetGPUDescriptorHandleForHeapStart(), raytracing_descriptor_index, descriptor_size);
}

void Raytracer::CreateDescriptorHeap()
{
	auto device = device_resources->GetD3DDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
	descriptor_heap_desc.NumDescriptors = 3;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;
	device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));
	NAME_D3D12_OBJECT(descriptor_heap);

	descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Raytracer::BuildGeometry()
{
	auto device = device_resources->GetD3DDevice();

	// Cube indices.
	Index indices[] =
	{
		3,1,0,
		2,1,3,

		6,4,5,
		7,4,6,

		11,9,8,
		10,9,11,

		14,12,13,
		15,12,14,

		19,17,16,
		18,17,19,

		22,20,21,
		23,20,22
	};

	// Cube vertices positions and corresponding triangle normals.
	Vertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	};

	AllocateUploadBuffer(device, indices, sizeof(indices), &index_buffer.resource);
	AllocateUploadBuffer(device, vertices, sizeof(vertices), &vertex_buffer.resource);


	// Vertex buffer is passed to the shader along with index buffer as a descriptor table.
	// Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
	UINT descriptorIndexIB = CreateBufferSRV(&index_buffer, sizeof(indices) / 4, 0);
	UINT descriptorIndexVB = CreateBufferSRV(&vertex_buffer, ARRAYSIZE(vertices), sizeof(vertices[0]));
	ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
}

void Raytracer::BuildAccelerationStructures()
{
	auto device = device_resources->GetD3DDevice();
	auto commandList = device_resources->GetCommandList();
	auto commandQueue = device_resources->GetCommandQueue();
	auto commandAllocator = device_resources->GetCommandAllocator();

	// Reset the command list for the acceleration structure construction.
	commandList->Reset(commandAllocator, nullptr);

	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = index_buffer.resource->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(index_buffer.resource->GetDesc().Width) / sizeof(Index);
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
	geometryDesc.Triangles.Transform3x4 = 0;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(vertex_buffer.resource->GetDesc().Width) / sizeof(Vertex);
	geometryDesc.Triangles.VertexBuffer.StartAddress = vertex_buffer.resource->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = buildFlags;
	bottomLevelInputs.NumDescs = 1;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = &geometryDesc;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = buildFlags;
	topLevelInputs.NumDescs = 1;
	topLevelInputs.pGeometryDescs = nullptr;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	dxr_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	ComPtr<ID3D12Resource> scratchResource;
	AllocateUAVBuffer(device, max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes), &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");
		AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
	}

	// Create an instance desc for the bottom-level acceleration structure.
	ComPtr<ID3D12Resource> instanceDescs;
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
	instanceDesc.InstanceMask = 1;
	instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
	AllocateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

	// Bottom Level Acceleration Structure desc
	{
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
	}

	// Top Level Acceleration Structure desc
	{
		topLevelBuildDesc.DestAccelerationStructureData = m_topLevelAccelerationStructure->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		topLevelBuildDesc.Inputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
	}

	auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
	{
		raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure.Get()));
		raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	};

	// Build acceleration structure.
	BuildAccelerationStructure(dxr_command_list.Get());

	// Kick off acceleration structure construction.
	device_resources->ExecuteCommandList();

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	device_resources->WaitForGpu();
}

void Raytracer::BuildShaderTables()
{
	auto device = device_resources->GetD3DDevice();

	void* rayGenShaderIdentifier;
	void* missShaderIdentifier;
	void* hitGroupShaderIdentifier;

	auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
	{
		rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(ray_shader_name);
		missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(miss_shader_name);
		hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(hit_group_name);
	};

	// Get shader identifiers.
	UINT shaderIdentifierSize;
	{
		ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		ThrowIfFailed(dxr_state_object.As(&stateObjectProperties));
		GetShaderIdentifiers(stateObjectProperties.Get());
		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
		ray_shader_table = rayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
		miss_shader_table = missShaderTable.GetResource();
	}

	// Hit group shader table
	{
		struct RootArguments {
			CubeConstantBuffer cb;
		} rootArguments;
		rootArguments.cb = m_cubeCB;

		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
		ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
		hit_group_table = hitGroupShaderTable.GetResource();
	}
}

void Raytracer::RaytraceScene()
{
	auto commandList = device_resources->GetCommandList();
	auto frameIndex = device_resources->GetCurrentFrameIndex();

	auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
	{
		// Since each shader table has only one shader record, the stride is same as the size.
		dispatchDesc->HitGroupTable.StartAddress = hit_group_table->GetGPUVirtualAddress();
		dispatchDesc->HitGroupTable.SizeInBytes = hit_group_table->GetDesc().Width;
		dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
		dispatchDesc->MissShaderTable.StartAddress = miss_shader_table->GetGPUVirtualAddress();
		dispatchDesc->MissShaderTable.SizeInBytes = miss_shader_table->GetDesc().Width;
		dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
		dispatchDesc->RayGenerationShaderRecord.StartAddress = ray_shader_table->GetGPUVirtualAddress();
		dispatchDesc->RayGenerationShaderRecord.SizeInBytes = ray_shader_table->GetDesc().Width;
		dispatchDesc->Width = system->GetWidth();
		dispatchDesc->Height = system->GetHeight();
		dispatchDesc->Depth = 1;
		commandList->SetPipelineState1(stateObject);
		commandList->DispatchRays(dispatchDesc);
	};

	auto SetCommonPipelineState = [&](auto* descriptorSetCommandList)
	{
		descriptorSetCommandList->SetDescriptorHeaps(1, descriptor_heap.GetAddressOf());
		// Set index and successive vertex buffer decriptor tables
		commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VertexBuffersSlot, index_buffer.gpuDescriptorHandle);
		commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, raytracing_gpu_descriptor);
	};

	commandList->SetComputeRootSignature(raytrace_global_root_signature.Get());

	// Copy the updated scene constant buffer to GPU.
	memcpy(&mapped_constants[frameIndex].constants, &m_sceneCB[frameIndex], sizeof(m_sceneCB[frameIndex]));
	auto cbGpuAddress = per_frame_constant->GetGPUVirtualAddress() + frameIndex * sizeof(mapped_constants[0]);
	commandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstantSlot, cbGpuAddress);

	// Bind the heaps, acceleration structure and dispatch rays.
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	SetCommonPipelineState(commandList);
	commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_topLevelAccelerationStructure->GetGPUVirtualAddress());
	DispatchRays(dxr_command_list.Get(), dxr_state_object.Get(), &dispatchDesc);
}

void Raytracer::CopyRaytracingOutputToBackbuffer()
{
	auto commandList = device_resources->GetCommandList();
	auto renderTarget = device_resources->GetRenderTarget();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracing_output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	commandList->CopyResource(renderTarget, raytracing_output.Get());

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracing_output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

void Raytracer::CreateWindowSizeDependentResources()
{
	CreateRaytracingOutputResource();
	//UpdateCameraMatrices();
}

void Raytracer::ReleaseWindowSizeDependentResources()
{
	raytracing_output.Reset();
}

void Raytracer::ReleaseDeviceDependentResources()
{
	raytrace_global_root_signature.Reset();
	raytrace_local_root_signature.Reset();

	dxr_device.Reset();
	dxr_command_list.Reset();
	dxr_state_object.Reset();

	descriptor_heap.Reset();
	descriptors_allocated = 0;
	raytracing_descriptor_index = UINT_MAX;
	index_buffer.resource.Reset();
	vertex_buffer.resource.Reset();
	per_frame_constant.Reset();
	ray_shader_table.Reset();
	miss_shader_table.Reset();
	hit_group_table.Reset();

	m_bottomLevelAccelerationStructure.Reset();
	m_topLevelAccelerationStructure.Reset();

}

void Raytracer::RecreateD3D()
{
	// Give GPU a chance to finish its execution in progress.
	try
	{
		device_resources->WaitForGpu();
	}
	catch (HrException&)
	{
		// Do nothing, currently attached adapter is unresponsive.
	}
	device_resources->HandleDeviceLost();
}

void Raytracer::Render()
{
	if (!device_resources->IsWindowVisible())
	{
		return;
	}

	device_resources->Prepare();
	RaytraceScene();
	CopyRaytracingOutputToBackbuffer();

	device_resources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void Raytracer::Destroy()
{
	// Let GPU finish before releasing D3D resources.
	device_resources->WaitForGpu();
	DeviceLost();
}

// Release all device dependent resouces when a device is lost.
void Raytracer::DeviceLost()
{
	ReleaseWindowSizeDependentResources();
	ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void Raytracer::DeviceRestored()
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

void Raytracer::CalculateFrameStats()
{
	static int frameCnt = 0;
	static double elapsedTime = 0.0f;
	double totalTime =  0; //m_timer.GetTotalSeconds();
	frameCnt++;

	// Compute averages over one second period.
	if ((totalTime - elapsedTime) >= 1.0f)
	{
		float diff = static_cast<float>(totalTime - elapsedTime);
		float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

		frameCnt = 0;
		elapsedTime = totalTime;

		float MRaysPerSecond = (system->GetWidth() * system->GetHeight() * fps) / static_cast<float>(1e6);

		std::wstringstream windowText;
		windowText << std::setprecision(2) << std::fixed
			<< L"    fps: " << fps << L"     ~Million Primary Rays/s: " << MRaysPerSecond
			<< L"    GPU[" << device_resources->GetAdapterID() << L"]: " << device_resources->GetAdapterDescription();
		system->SetCustomWindowText(windowText.str().c_str());
	}
}

void Raytracer::OnSizeChanged(UINT width, UINT height, bool minimized)
{
	if (!device_resources->WindowSizeChanged(width, height, minimized))
	{
		return;
	}

	UpdateForSizeChange(width, height);

	ReleaseWindowSizeDependentResources();
	CreateWindowSizeDependentResources();
}

UINT Raytracer::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
	auto descriptorHeapCpuBase = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	if (descriptorIndexToUse >= descriptor_heap->GetDesc().NumDescriptors)
	{
		descriptorIndexToUse = descriptors_allocated++;
	}
	*cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, descriptor_size);
	return descriptorIndexToUse;
}

UINT Raytracer::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
{
	auto device = device_resources->GetD3DDevice();

	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = numElements;
	if (elementSize == 0)
	{
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.StructureByteStride = 0;
	}
	else
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Buffer.StructureByteStride = elementSize;
	}
	UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
	device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
	buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_heap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, descriptor_size);
	return descriptorIndex;
}