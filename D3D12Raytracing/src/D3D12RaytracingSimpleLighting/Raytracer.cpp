#include "stdafx.h"
#include "Raytracer.h"
#include "DirectXRaytracingHelper.h"
#include "Raytracing.hlsl.h"
#include "RayTracingHlslCompat.h"
#include "Scene.h"
#include "Portal.h"

using namespace DirectX;

const wchar_t* Raytracer::hit_group_name[2] =
{
    L"MyHitGroup",
    L"PortalHitGroup"
};

const wchar_t* Raytracer::raygen_shader_name = L"MyRaygenShader";
const wchar_t* Raytracer::closest_hit_shader_names[2] =
{
    L"MyClosestHitShader",
    L"PortalClosestHitShader"
};


const wchar_t* Raytracer::miss_shader_name = L"MyMissShader";

Raytracer::Raytracer(UINT heap_index)
{
    descriptor_heap_index = heap_index;
}

Raytracer::~Raytracer() {}

void Raytracer::CreateRaytracingInterfaces(DX::DeviceResources *device_resources)
{
    auto device = device_resources->GetD3DDevice();
    auto commandList = device_resources->GetCommandList();

    ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxr_device)), L"Couldn't get DirectX Raytracing interface for the device.\n");
    ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxr_commandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

void Raytracer::CreateRootSignatures(DX::DeviceResources *device_resources)
{
    auto device = device_resources->GetD3DDevice();

  
        CD3DX12_DESCRIPTOR_RANGE ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // Index + Vertex Buffers

        // Global Root Signature
        CD3DX12_ROOT_PARAMETER globalParams[GlobalRootSignatureParams::Count];
        globalParams[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
        globalParams[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
        globalParams[GlobalRootSignatureParams::SceneConstantSlot].InitAsConstantBufferView(0);
        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(globalParams), globalParams);
        SerializeAndCreateRaytracingRootSignature(device_resources, globalRootSignatureDesc, &raytracing_global_root_signature);

        // Local Root Signature
        CD3DX12_ROOT_PARAMETER localParams[LocalRootSignatureParams::Count];
        localParams[LocalRootSignatureParams::CubeConstantSlot].InitAsConstants(SizeOfInUint32(object_constant_buffer), 1);
        localParams[LocalRootSignatureParams::PortalSlot].InitAsConstants(SizeOfInUint32(portal_constant_buffer), 2);
        localParams[LocalRootSignatureParams::VertexBuffersSlot].InitAsDescriptorTable(1, &ranges[1]);
        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(localParams), localParams);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        SerializeAndCreateRaytracingRootSignature(device_resources, localRootSignatureDesc, &raytracing_local_root_signature);
    
}

void Raytracer::SerializeAndCreateRaytracingRootSignature(DX::DeviceResources *device_resources, D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = device_resources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

void Raytracer::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Ray gen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

    // Local root signature to be used in a hit group.
    auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSignature->SetRootSignature(raytracing_local_root_signature.Get());
    // Define explicit shader association for the local root signature. 
    {
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExports(hit_group_name);
    }
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void Raytracer::CreateRaytracingPipelineStateObject()
{
    // Create 7 subobjects that combine into a RTPSO:
    // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
    // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
    // This simple sample utilizes default shader association except for local root signature subobject
    // which has an explicit association specified purely for demonstration purposes.
    // 1 - DXIL library
    // 1 - Triangle hit group
    // 1 - Shader config
    // 2 - Local root signature and association
    // 1 - Global root signature
    // 1 - Pipeline config
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
        lib->DefineExport(raygen_shader_name);
        lib->DefineExport(closest_hit_shader_names[0]);
        lib->DefineExport(closest_hit_shader_names[1]);
        lib->DefineExport(miss_shader_name);
    }

    // Triangle hit group
    // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
    // In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
    auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetClosestHitShaderImport(closest_hit_shader_names[0]);
    hitGroup->SetHitGroupExport(hit_group_name[0]);
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

    auto hitGroup2 = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup2->SetClosestHitShaderImport(closest_hit_shader_names[1]);
    hitGroup2->SetHitGroupExport(hit_group_name[1]);
    hitGroup2->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

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
    globalRootSignature->SetRootSignature(raytracing_global_root_signature.Get());

    // Pipeline config
    // Defines the maximum TraceRay() recursion depth.
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    // PERFOMANCE TIP: Set max recursion depth as low as needed 
    // as drivers may apply optimization strategies for low recursion depths.
    UINT maxRecursionDepth = 12; // ~ primary rays only. 
    pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
    PrintStateObjectDesc(raytracingPipeline);
#endif

    // Create the state object.
    ThrowIfFailed(dxr_device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&dxr_state_object)), L"Couldn't create DirectX Raytracing state object.\n");
}

// Create 2D output texture for raytracing.
void Raytracer::CreateRaytracingOutputResource(DX::DeviceResources* device_resources, UINT64 width, UINT64 height)
{
    auto device = device_resources->GetD3DDevice();
    auto backbufferFormat = device_resources->GetBackBufferFormat();

    // Create the output resource. The dimensions and format should match the swap-chain.
    auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracing_output)));
    NAME_D3D12_OBJECT(raytracing_output);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
    descriptor_heap_index = AllocateDescriptor(&uavDescriptorHandle, descriptor_heap_index);
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(raytracing_output.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
    raytracing_output_resource_UAV_gpu_descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_heap->GetGPUDescriptorHandleForHeapStart(), descriptor_heap_index, descriptor_size);
}

//void Raytracer::BuildGeometryBuffers(DX::DeviceResources* device_resources, Raytracer* raytracer, Scene* scene)
//{
//    auto device = device_resources->GetD3DDevice();
//
//    //// Cube indices.
//    int i_count = scene->GetSceneIndices().size();
//    int v_count = scene->GetSceneVertices().size();
//
//    AllocateUploadBuffer(device, scene->GetSceneIndices().data(), i_count * sizeof(int), &raytracer->GetIndexBuffer()->resource);
//    AllocateUploadBuffer(device, scene->GetSceneVertices().data(), v_count * sizeof(Vertex), &raytracer->GetVertexBuffer()->resource);
//
//    // Vertex buffer is passed to the shader along with index buffer as a descriptor table.
//    // Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
//    UINT descriptorIndexIB = raytracer->CreateBufferSRV(device_resources, raytracer->GetIndexBuffer(), (i_count * 2) / 4, 0);
//    UINT descriptorIndexVB = raytracer->CreateBufferSRV(device_resources, raytracer->GetVertexBuffer(), v_count, sizeof(XMFLOAT3) * 2);
//    ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
//}

void Raytracer::CreateDescriptorHeap(DX::DeviceResources* device_resources, int buffer_count)
{
    auto device = device_resources->GetD3DDevice();

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    // Allocate a heap for 3 descriptors:
    // 2 - vertex and index buffer SRVs
    // 1 - raytracing output texture SRV
    descriptorHeapDesc.NumDescriptors = 1 + (buffer_count * 2); // 1 raytracing output + 2 slots for index and vertex buffers for each model
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptor_heap));
    NAME_D3D12_OBJECT(descriptor_heap);

    descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

UINT Raytracer::CreateBufferSRV(DX::DeviceResources* device_resources, D3DBuffer* buffer, UINT numElements, UINT elementSize)
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

void Raytracer::DoRaytracing(DX::DeviceResources* device_resourcecs, UINT width, UINT height, ComPtr<ID3D12Resource> m_topLevelAccelerationStructure)
{
    auto commandList = device_resourcecs->GetCommandList();
    UINT frameIndex = device_resourcecs->GetCurrentFrameIndex();

    auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
    {
        // Since each shader table has only one shader record, the stride is same as the size.
        dispatchDesc->HitGroupTable.StartAddress = hit_group_shader_table->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = hit_group_shader_table->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = hit_group_stride_in_bytes;
        dispatchDesc->MissShaderTable.StartAddress = miss_shader_table->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = miss_shader_table->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = ray_gen_shader_table->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = ray_gen_shader_table->GetDesc().Width;
        dispatchDesc->Width = width;
        dispatchDesc->Height = height;
        dispatchDesc->Depth = 1;
        commandList->SetPipelineState1(stateObject);
        commandList->DispatchRays(dispatchDesc);
    };

    commandList->SetComputeRootSignature(raytracing_global_root_signature.Get());

    // Copy the updated scene constant buffer to GPU.
    memcpy(&mapped_constant_data[frameIndex].constants, &scene_constant_buffers[frameIndex], sizeof(scene_constant_buffers[frameIndex]));
    auto cbGpuAddress = per_frame_constants->GetGPUVirtualAddress() + frameIndex * sizeof(mapped_constant_data[0]);
    commandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstantSlot, cbGpuAddress);

    // Bind the heaps, acceleration structure and dispatch rays.
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};

    commandList->SetDescriptorHeaps(1, descriptor_heap.GetAddressOf());

    commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, raytracing_output_resource_UAV_gpu_descriptor);
    commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_topLevelAccelerationStructure->GetGPUVirtualAddress());

    DispatchRays(dxr_commandList.Get(), dxr_state_object.Get(), &dispatchDesc);

}

void Raytracer::BuildShaderTables(DX::DeviceResources* device_resources, std::vector<Portal> portals)
{
    auto device = device_resources->GetD3DDevice();

    void* rayGenShaderIdentifier;
    void* missShaderIdentifier;
    void* hitGroupShaderIdentifier[2];

    auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
    {
        rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(raygen_shader_name);
        missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(miss_shader_name);
        hitGroupShaderIdentifier[0] = stateObjectProperties->GetShaderIdentifier(hit_group_name[0]);
        hitGroupShaderIdentifier[1] = stateObjectProperties->GetShaderIdentifier(hit_group_name[1]);
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
        ray_gen_shader_table = rayGenShaderTable.GetResource();
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

    //needs redoing for performance
    int hitgroup_counts = index_buffer.size();
    std::vector<RootArguments> root_arguments;
    ObjectConstantBuffer CubeCB1, CubeCB2, CubeCB3, CubeCB4, CubeCB5;
    PortalSlot portal1_CB, portal2_CB;

    //portals
    CubeCB1.albedo = object_constant_buffer.albedo;
    CubeCB1.origin_position = portals[0].GetPortalOrigin();
    portal1_CB.link_position = portals[1].GetPortalOrigin();
    CubeCB2.albedo = object_constant_buffer.albedo;
    CubeCB2.origin_position = portals[1].GetPortalOrigin();
    portal2_CB.link_position = portals[0].GetPortalOrigin();

    //object colours
    CubeCB3.albedo = XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f);
    CubeCB4.albedo = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    CubeCB5.albedo = XMFLOAT4(0.2f, 1.0f, 0.2f, 1.0f);


    //settings the portals manually, should be done automatically in future with reference to portal buffers
    for (int i = 0; i < hitgroup_counts; i++)
    {
        RootArguments new_root;
        new_root.constant_buffer = object_constant_buffer;
        if (i == 0)
        {
            new_root.constant_buffer = CubeCB3;
        }
        if (i == 1)
        {
            new_root.constant_buffer = CubeCB4;
        }
        if (i == 2)
        {
            new_root.constant_buffer = CubeCB5;
        }
        if (i == 14)
        {
            new_root.constant_buffer = CubeCB1;
            new_root.portal_slot = portal1_CB;
        }
        if (i == 15)
        {
            new_root.constant_buffer = CubeCB2;
            new_root.portal_slot = portal2_CB;
        }
        new_root.gpu_handle = index_buffer[i].gpuDescriptorHandle.ptr;
        root_arguments.push_back(new_root);
    }


#define ALIGN(alignment, num) ((((num) + alignment - 1) / alignment) * alignment)
    const UINT offsetToDescriptorHandle = ALIGN(sizeof(D3D12_GPU_DESCRIPTOR_HANDLE), shaderIdentifierSize);
    const UINT offsetToMaterialConstants = ALIGN(sizeof(UINT32), offsetToDescriptorHandle + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
    const UINT shaderRecordSizeInBytes = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, offsetToMaterialConstants + sizeof(RootArguments));

    UINT numShaderRecords = hitgroup_counts;
    UINT shaderRecordSize = shaderRecordSizeInBytes;
    hit_group_stride_in_bytes = shaderRecordSize;
    ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
    for (int i = 0; i < hitgroup_counts; i++)
    {
        
        if (i == 14 || i == 15)
        {
            hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier[1], shaderIdentifierSize, &root_arguments[i], sizeof(root_arguments[i])));
        }
        else
        {
            hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier[0], shaderIdentifierSize, &root_arguments[i], sizeof(root_arguments[i])));
        }
    }
    hit_group_shader_table = hitGroupShaderTable.GetResource();
    
}

void Raytracer::CopyRaytracingOutputToBackbuffer(DX::DeviceResources* device_resources)
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

void Raytracer::CreateConstantBuffers(DX::DeviceResources* device_resources)
{
    auto device = device_resources->GetD3DDevice();
    auto frameCount = device_resources->GetBackBufferCount();

    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = frameCount * sizeof(AlignedSceneConstantBuffer);
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&per_frame_constants)));

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(per_frame_constants->Map(0, nullptr, reinterpret_cast<void**>(&mapped_constant_data)));
}

void Raytracer::ReleaseRaytracerResources()
{
    raytracing_global_root_signature.Reset();
    raytracing_local_root_signature.Reset();

    dxr_device.Reset();
    dxr_commandList.Reset();
    dxr_state_object.Reset();

    descriptor_heap.Reset();
    descriptors_allocated = 0;
    descriptor_heap_index = UINT_MAX;
    for (int i = 0; i < index_buffer.size(); i++)
    {
        index_buffer[i].resource.Reset();
    }
    for (int i = 0; i < vertex_buffer.size(); i++)
    {
        vertex_buffer[i].resource.Reset();
    }
    per_frame_constants.Reset();
    ray_gen_shader_table.Reset();
    miss_shader_table.Reset();
    hit_group_shader_table.Reset();
}