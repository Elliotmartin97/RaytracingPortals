//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "stdafx.h"
#include "D3D12RaytracingSimpleLighting.h"
#include "DirectXRaytracingHelper.h"
#include "Raytracing.hlsl.h"
#include "Raytracer.h"
#include "AccelerationStructure.h"
#include "Model.h"
#include "Scene.h"
#include "Camera.h"

using namespace std;
using namespace DX;

D3D12RaytracingSimpleLighting::D3D12RaytracingSimpleLighting(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name)
{
    m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
    raytracer = new Raytracer(m_raytracingOutputResourceUAVDescriptorHeapIndex);
    acceleration_structure = new AccelerationStructure();
    scene = new Scene();
    camera = new Camera();
    UpdateForSizeChange(width, height);
}

void D3D12RaytracingSimpleLighting::OnInit()
{
    m_deviceResources = std::make_unique<DeviceResources>(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN,
        FrameCount,
        D3D_FEATURE_LEVEL_11_0,
        // Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
        // Since the sample requires build 1809 (RS5) or higher, we don't need to handle non-tearing cases.
        DeviceResources::c_RequireTearingSupport,
        m_adapterIDoverride
        );
    m_deviceResources->RegisterDeviceNotify(this);
    m_deviceResources->SetWindow(Win32Application::GetHwnd(), m_width, m_height);
    m_deviceResources->InitializeDXGIAdapter();

    ThrowIfFalse(IsDirectXRaytracingSupported(m_deviceResources->GetAdapter()),
        L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    InitializeScene();

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Initialize scene rendering parameters.
void D3D12RaytracingSimpleLighting::InitializeScene()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    // Setup materials.
    {
        raytracer->GetCubeCB().albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Setup camera.
    {
        // Initialize the view and projection inverse matrices.
        camera->SetPosition(0.0f, 2.0f, -9.0f, 1.0f);
        camera->SetTarget(0.0f, 0.0f, 0.0f, 1.0f);
        camera->SetupCamera();

        
        camera->UpdateCameraMatrices(m_deviceResources.get(), raytracer, m_aspectRatio);
    }

    // Setup lights.
    {
        // Initialize the lighting parameters.
        XMFLOAT4 lightPosition;
        XMFLOAT4 lightAmbientColor;
        XMFLOAT4 lightDiffuseColor;

        lightPosition = XMFLOAT4(4.0f, 0.0f, 0.0, 0.0f);
        raytracer->GetSceneCB()[frameIndex].lightPosition = XMLoadFloat4(&lightPosition);

        lightAmbientColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        raytracer->GetSceneCB()[frameIndex].lightAmbientColor = XMLoadFloat4(&lightAmbientColor);

        lightDiffuseColor = XMFLOAT4(0.0f, 0.5f, 0.0f, 1.0f);
        raytracer->GetSceneCB()[frameIndex].lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
    }

    // Apply the initial values to all frames' buffer instances.
    for (int i = 0; i < raytracer->GetFrameCount(); i++)
    {
        raytracer->GetSceneCB()[i] = raytracer->GetSceneCB()[frameIndex];
    }
}

// Create resources that depend on the device.
void D3D12RaytracingSimpleLighting::CreateDeviceDependentResources()
{
    // Initialize raytracing pipeline.

    // Create raytracing interfaces: raytracing device and commandlist.
    raytracer->CreateRaytracingInterfaces(m_deviceResources.get());

    // Create root signatures for the shaders.
    raytracer->CreateRootSignatures(m_deviceResources.get());

    // Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
    raytracer->CreateRaytracingPipelineStateObject();

    // Create a heap for descriptors.
    raytracer->CreateDescriptorHeap(m_deviceResources.get(), scene->GetSceneModelCount("Scenes/Scene0.txt"));
    // Build geometry to be used in the sample.
    scene->LoadScene(m_deviceResources.get(), raytracer, "Scenes/scene0.txt");

   // raytracer->BuildGeometryBuffers(m_deviceResources.get(), raytracer, scene);

    acceleration_structure->BuildAccelerationStructures(raytracer, m_deviceResources.get(), scene);


    // Create constant buffers for the geometry and the scene.
    raytracer->CreateConstantBuffers(m_deviceResources.get());

    // Build shader tables, which define shaders and their local root arguments.
    raytracer->BuildShaderTables(m_deviceResources.get());

    // Create an output 2D texture to store the raytracing result to.
    raytracer->CreateRaytracingOutputResource(m_deviceResources.get(), m_width, m_height);
}

void D3D12RaytracingSimpleLighting::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = m_deviceResources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

// Update frame-based values.
void D3D12RaytracingSimpleLighting::OnUpdate()
{
    m_timer.Tick();
    CalculateFrameStats();
    float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
    auto prevFrameIndex = m_deviceResources->GetPreviousFrameIndex();

    // Rotate the camera around Y axis.
    {
   /*     float secondsToRotateAround = 56.0f;
        float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        camera->RotatePosition(rotate);
        camera->RotateTarget(rotate);
        camera->RotateUp(rotate);
        camera->UpdateCameraMatrices(m_deviceResources.get(), raytracer, m_aspectRatio);*/
    }

    // Rotate the second light around Y axis.
    {
        float secondsToRotateAround = 8.0f;
        float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        const XMVECTOR& prevLightPosition = raytracer->GetSceneCB()[prevFrameIndex].lightPosition;
        raytracer->GetSceneCB()[frameIndex].lightPosition = XMVector3Transform(prevLightPosition, rotate);
    }
}

// Update the application state with the new resolution.
void D3D12RaytracingSimpleLighting::UpdateForSizeChange(UINT width, UINT height)
{
    DXSample::UpdateForSizeChange(width, height);
}


// Create resources that are dependent on the size of the main window.
void D3D12RaytracingSimpleLighting::CreateWindowSizeDependentResources()
{
    raytracer->CreateRaytracingOutputResource(m_deviceResources.get(),m_width, m_height);
    camera->UpdateCameraMatrices(m_deviceResources.get(), raytracer, m_aspectRatio);
}

// Release resources that are dependent on the size of the main window.
void D3D12RaytracingSimpleLighting::ReleaseWindowSizeDependentResources()
{
    raytracer->GetRaytracingOutput().Reset();
}

// Release all resources that depend on the device.
void D3D12RaytracingSimpleLighting::ReleaseDeviceDependentResources()
{
    raytracer->ReleaseRaytracerResources();
    acceleration_structure->ReleaseStructures();

}

void D3D12RaytracingSimpleLighting::RecreateD3D()
{
    // Give GPU a chance to finish its execution in progress.
    try
    {
        m_deviceResources->WaitForGpu();
    }
    catch (HrException&)
    {
        // Do nothing, currently attached adapter is unresponsive.
    }
    m_deviceResources->HandleDeviceLost();
}

// Render the scene.
void D3D12RaytracingSimpleLighting::OnRender()
{
    if (!m_deviceResources->IsWindowVisible())
    {
        return;
    }

    m_deviceResources->Prepare();
    raytracer->DoRaytracing(m_deviceResources.get(), m_width, m_height, acceleration_structure->GetTopLevelStructure());
    raytracer->CopyRaytracingOutputToBackbuffer(m_deviceResources.get());

    m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void D3D12RaytracingSimpleLighting::OnDestroy()
{
    // Let GPU finish before releasing D3D resources.
    m_deviceResources->WaitForGpu();
    OnDeviceLost();

    delete acceleration_structure;
    acceleration_structure = nullptr;
    delete raytracer;
    raytracer = nullptr;
    delete scene;
    scene = nullptr;
    delete camera;
    camera = nullptr;
}

// Release all device dependent resouces when a device is lost.
void D3D12RaytracingSimpleLighting::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void D3D12RaytracingSimpleLighting::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void D3D12RaytracingSimpleLighting::CalculateFrameStats()
{
    static int frameCnt = 0;
    static double elapsedTime = 0.0f;
    double totalTime = m_timer.GetTotalSeconds();
    frameCnt++;

    // Compute averages over one second period.
    if ((totalTime - elapsedTime) >= 1.0f)
    {
        float diff = static_cast<float>(totalTime - elapsedTime);
        float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

        frameCnt = 0;
        elapsedTime = totalTime;

        float MRaysPerSecond = (m_width * m_height * fps) / static_cast<float>(1e6);

        wstringstream windowText;
        windowText << setprecision(2) << fixed
            << L"    fps: " << fps << L"     ~Million Primary Rays/s: " << MRaysPerSecond
            << L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

// Handle OnSizeChanged message event.
void D3D12RaytracingSimpleLighting::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}