#include "stdafx.h"
#include "DirectXRaytracingHelper.h"
#include "AccelerationStructure.h"
#include "Raytracer.h"

AccelerationStructure::AccelerationStructure() {}

AccelerationStructure::~AccelerationStructure() {}

void AccelerationStructure::BuildGeometryDescsForBottomLevelAS(Raytracer* raytracer, std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, std::vector<int> index_counts,
    std::vector<int> vertex_counts, std::vector<int> index_starts, std::vector<int> vertex_starts)
{
    for (int i = 0; i < geometryDescs.size(); i++)
    {
        geometryDescs[i] = {};
        geometryDescs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDescs[i].Triangles.IndexBuffer = raytracer->GetIndexBuffer()->resource->GetGPUVirtualAddress() + index_starts[i] * 2;
        geometryDescs[i].Triangles.IndexCount = index_counts[i];
        geometryDescs[i].Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        geometryDescs[i].Triangles.Transform3x4 = 0;
        geometryDescs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDescs[i].Triangles.VertexCount = vertex_counts[i];
        geometryDescs[i].Triangles.VertexBuffer.StartAddress = raytracer->GetVertexBuffer()->resource->GetGPUVirtualAddress() + vertex_starts[i] * 24;
        geometryDescs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
        geometryDescs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    }
}

void AccelerationStructure::BuildBottomLevelASInstanceDescs(DX::DeviceResources* device_resources, std::vector<D3D12_GPU_VIRTUAL_ADDRESS> bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource)
{
    auto device = device_resources->GetD3DDevice();

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(bottomLevelASaddresses.size());

    for (int i = 0; i < instanceDescs.size(); i++)
    {
        instanceDescs[i] = {};
        instanceDescs[i].InstanceMask = 1;
        instanceDescs[i].InstanceContributionToHitGroupIndex = 0;
        instanceDescs[i].AccelerationStructure = bottomLevelASaddresses[i];
        XMFLOAT3 float_position = XMFLOAT3( 0.0f, 0.0f, i * 3.0f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&float_position);

        // Scale in XZ dimensions.
        XMMATRIX mScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mTransform = mScale * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDescs[i].Transform), mTransform);
    }
    UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
    AllocateUploadBuffer(device, instanceDescs.data(), bufferSize, &(*instanceDescsResource), L"InstanceDescs");
}

AccelerationStructureBuffers AccelerationStructure::BuildTopLevelAS(DX::DeviceResources* device_resources, Raytracer* raytracer,
    std::vector<int> index_counts, std::vector<int> vertex_counts, int num_blas, std::vector<AccelerationStructureBuffers> bottomLevelAS)
{
    auto device = device_resources->GetD3DDevice();
    auto commandList = device_resources->GetCommandList();
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> topLevelAS;

    // Get required sizes for an acceleration structure.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = num_blas;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
    raytracer->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    // Allocate resources for acceleration structures.
    // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
    // Default heap is OK since the application doesn’t need CPU read/write access to them. 
    // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
    // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
    //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
    //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
    {
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &topLevelAS, initialResourceState, L"TopLevelAccelerationStructure");
    }

    // Create instance descs for the bottom-level acceleration structures.
    ComPtr<ID3D12Resource> instanceDescsResource;
    std::vector<D3D12_GPU_VIRTUAL_ADDRESS> bottomLevelASaddresses(num_blas);
    for (int i = 0; i < num_blas; i++)
    {
        bottomLevelASaddresses[i] = bottomLevelAS[i].accelerationStructure->GetGPUVirtualAddress();
    }
        
    BuildBottomLevelASInstanceDescs(device_resources, bottomLevelASaddresses, &instanceDescsResource);
    

    // Top-level AS desc
    {
        topLevelBuildDesc.DestAccelerationStructureData = topLevelAS->GetGPUVirtualAddress();
        topLevelInputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();
        topLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
    }

    // Build acceleration structure.
    raytracer->GetCMDList()->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    AccelerationStructureBuffers topLevelASBuffers;
    topLevelASBuffers.accelerationStructure = topLevelAS;
    topLevelASBuffers.instanceDesc = instanceDescsResource;
    topLevelASBuffers.scratch = scratch;
    topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    return topLevelASBuffers;
}

AccelerationStructureBuffers AccelerationStructure::BuildBottomLevelAS(DX::DeviceResources* device_resources, Raytracer* raytracer, D3D12_RAYTRACING_GEOMETRY_DESC geometryDescs)
{
    auto device = device_resources->GetD3DDevice();
    auto commandList = device_resources->GetCommandList();
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> bottomLevelAS;

    // Get the size requirements for the scratch and AS buffers.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
    bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    bottomLevelInputs.Flags = buildFlags;
    bottomLevelInputs.NumDescs = 1;
    bottomLevelInputs.pGeometryDescs = &geometryDescs;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    raytracer->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    // Create a scratch buffer.
    AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    {
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState, L"BottomLevelAccelerationStructure");
    }

    // bottom-level AS desc.
    {
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->GetGPUVirtualAddress();
    }

    // Build the acceleration structure.
    raytracer->GetCMDList()->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

    AccelerationStructureBuffers bottomLevelASBuffers;
    bottomLevelASBuffers.accelerationStructure = bottomLevelAS;
    bottomLevelASBuffers.scratch = scratch;
    bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    return bottomLevelASBuffers;
}

void AccelerationStructure::BuildAccelerationStructures(Raytracer* raytracer, DX::DeviceResources* device_resources, std::vector<int> index_counts,
    std::vector<int> vertex_counts, std::vector<int> index_starts, std::vector<int> vertex_starts)
{
    auto device = device_resources->GetD3DDevice();
    auto commandList = device_resources->GetCommandList();
    auto commandQueue = device_resources->GetCommandQueue();
    auto commandAllocator = device_resources->GetCommandAllocator();

    int num_objects = index_counts.size();
    m_bottomLevelAccelerationStructure.resize(num_objects);
    // Reset the command list for the acceleration structure construction.
    commandList->Reset(commandAllocator, nullptr);

    // Build bottom-level AS.
    std::vector<AccelerationStructureBuffers> bottomLevelAS(num_objects);
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(num_objects);

    BuildGeometryDescsForBottomLevelAS(raytracer, geometryDescs, index_counts, vertex_counts, index_starts, vertex_starts);

    // Build all bottom-level AS.
    for (UINT i = 0; i < num_objects; i++)
    {
        bottomLevelAS[i] = BuildBottomLevelAS(device_resources, raytracer, geometryDescs[i]);
    }


    // Batch all resource barriers for bottom-level AS builds.
    std::vector<D3D12_RESOURCE_BARRIER> resourceBarriers(num_objects);
    for (UINT i = 0; i < num_objects; i++)
    {
        resourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS[i].accelerationStructure.Get());
    }
    commandList->ResourceBarrier(num_objects, &resourceBarriers[0]);

    // Build top-level AS.
    AccelerationStructureBuffers topLevelAS = BuildTopLevelAS(device_resources, raytracer, index_counts, vertex_counts, num_objects, bottomLevelAS);

    // Kick off acceleration structure construction.
    device_resources->ExecuteCommandList();

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    device_resources->WaitForGpu();

    // Store the AS buffers. The rest of the buffers will be released once we exit the function.
    for (UINT i = 0; i < num_objects; i++)
    {
        m_bottomLevelAccelerationStructure[i] = bottomLevelAS[i].accelerationStructure;
    }
    m_topLevelAccelerationStructure = topLevelAS.accelerationStructure;
}


void AccelerationStructure::ReleaseStructures()
{
    for (int i = 0; i < m_bottomLevelAccelerationStructure.size(); i++)
    {
        m_bottomLevelAccelerationStructure[i].Reset();
    }
    m_topLevelAccelerationStructure.Reset();
}