#include "stdafx.h"
#include "DirectXRaytracingHelper.h"
#include "AccelerationStructure.h"
#include "Raytracer.h"

AccelerationStructure::AccelerationStructure() {}

AccelerationStructure::~AccelerationStructure() {}

void AccelerationStructure::BuildAccelerationStructures(DX::DeviceResources* device_resources, Raytracer* raytracer)
{
    auto device = device_resources->GetD3DDevice();
    auto commandList = device_resources->GetCommandList();
    auto commandQueue = device_resources->GetCommandQueue();
    auto commandAllocator = device_resources->GetCommandAllocator();

    // Reset the command list for the acceleration structure construction.
    commandList->Reset(commandAllocator, nullptr);

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.IndexBuffer = raytracer->GetIndexBuffer()->resource->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount = static_cast<UINT>(raytracer->GetIndexBuffer()->resource->GetDesc().Width) / sizeof(Index);
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>(raytracer->GetVertexBuffer()->resource->GetDesc().Width) / sizeof(Vertex);
    geometryDesc.Triangles.VertexBuffer.StartAddress = raytracer->GetVertexBuffer()->resource->GetGPUVirtualAddress();
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

    int instance_count = 1; // number of duplicate instances of scene geometry, leave 1 for normal

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = instance_count;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
    raytracer->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    raytracer->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
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

    ComPtr<ID3D12Resource> instanceDescs;
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc[2] = {};
    

    for (int i = 0; i < instance_count; i++)
    {
        instanceDesc[i].InstanceID = i;
        instanceDesc[i].Transform[1][3] = i * 2;

        instanceDesc[i].Transform[0][0] = instanceDesc[i].Transform[1][1] = instanceDesc[i].Transform[2][2] = 1;
        instanceDesc[i].InstanceMask = 1;
        instanceDesc[i].AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
       
    }
    AllocateUploadBuffer(device, &instanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instance_count, &instanceDescs, L"InstanceDescs");

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

    {
        raytracer->GetCMDList()->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure.Get()));
        raytracer->GetCMDList()->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    }


    // Kick off acceleration structure construction.
    device_resources->ExecuteCommandList();

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    device_resources->WaitForGpu();
}

//void AccelerationStructure::BuildGeometryDescs(Raytracer &raytracer, std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs, std::vector<UINT> index_offsets, std::vector<UINT> vertex_offsets)
//{
//    D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
//    geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
//    geom.Triangles.IndexBuffer = raytracer.GetIndexBuffer().resource->GetGPUVirtualAddress();
//    geometry_descs[0].Triangles.VertexBuffer.StartAddress = raytracer.GetVertexBuffer().resource->GetGPUVirtualAddress();
//    for (int i = 0; i < geometry_descs.size(); i++)
//    {
//        geometry_descs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
//        geometry_descs[i].Triangles.IndexBuffer = raytracer.GetIndexBuffer().resource->GetGPUVirtualAddress() + index_offsets[i];
//        geometry_descs[i].Triangles.IndexCount = index_offsets[i] - 1;
//        geometry_descs[i].Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
//        geometry_descs[i].Triangles.Transform3x4 = 0;
//        geometry_descs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
//        geometry_descs[i].Triangles.VertexCount = vertex_offsets[i] - 1;
//        geometry_descs[i].Triangles.VertexBuffer.StartAddress = raytracer.GetVertexBuffer().resource->GetGPUVirtualAddress() + vertex_offsets[i];
//        geometry_descs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
//        geometry_descs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
//    }
//}

void AccelerationStructure::ReleaseStructures()
{
    m_bottomLevelAccelerationStructure.Reset();
    m_topLevelAccelerationStructure.Reset();
}