#include "stdafx.h"
#include "Graphics.h"
#include "DirectXRaytracingHelper.h"
#include "Raytracing.hlsl.h"
#include "Raytracer.h"
#include "AccelerationStructure.h"
#include "Model.h"
#include "Scene.h"
#include "Camera.h"
#include "Portal.h"

using namespace std;
using namespace DX;

Graphics::Graphics(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name)
{
    raytracing_output_resource_UAV_descriptor_heap_index = UINT_MAX;
    raytracer = new Raytracer(raytracing_output_resource_UAV_descriptor_heap_index);
    acceleration_structure = new AccelerationStructure();
    acceleration_structure = new AccelerationStructure();
    scene = new Scene();
    camera = new Camera();
    UpdateForSizeChange(width, height);
}

void Graphics::OnInit()
{
    device_resources = std::make_unique<DeviceResources>(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN,
        FRAME_COUNT,
        D3D_FEATURE_LEVEL_11_0,
        // Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
        // Since the sample requires build 1809 (RS5) or higher, we don't need to handle non-tearing cases.
        DeviceResources::c_RequireTearingSupport,
        m_adapterIDoverride
        );
    device_resources->RegisterDeviceNotify(this);
    device_resources->SetWindow(Win32Application::GetHwnd(), m_width, m_height);
    device_resources->InitializeDXGIAdapter();

    ThrowIfFalse(IsDirectXRaytracingSupported(device_resources->GetAdapter()),
        L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

    device_resources->CreateDeviceResources();
    device_resources->CreateWindowSizeDependentResources();

    InitializeScene();

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

void Graphics::SetWindowCenterPositions(int x, int y)
{
    window_center_x = x;
    window_center_y = y;
    if (camera != nullptr)
    {
        camera->SetMouseCenterPosition(x, y);
    }
}

// Initialize scene rendering parameters.
void Graphics::InitializeScene()
{
    auto frameIndex = device_resources->GetCurrentFrameIndex();

    // Setup materials.
    {
        raytracer->GetCubeCB().albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    camera->SetProjectionMatrix(60.0f, m_aspectRatio, 1.0f, 125.0f);
    camera->SetPositionFloat(0.0f, 0.0f, -10.0f);
    camera->SetRotationFloat(0.0f, 0.0f, 0.0f);
    camera->SetMouseCenterPosition(0, 0);
    camera->UpdateCameraMatrices(device_resources.get(), raytracer, m_aspectRatio);
    

    // Setup lights.
    XMFLOAT4 light_position;
    XMFLOAT4 light_ambient_color;
    XMFLOAT4 light_diffuse_color;

    light_position = XMFLOAT4(-7.5f, 0.0f, 0.0, 0.0f);
    raytracer->GetSceneCB()[frameIndex].lightPosition = XMLoadFloat4(&light_position);
    light_ambient_color = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
    raytracer->GetSceneCB()[frameIndex].lightAmbientColor = XMLoadFloat4(&light_ambient_color);
    light_diffuse_color = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    raytracer->GetSceneCB()[frameIndex].lightDiffuseColor = XMLoadFloat4(&light_diffuse_color);

    // Apply the initial values to all frames' buffer instances.
    for (int i = 0; i < raytracer->GetFrameCount(); i++)
    {
        raytracer->GetSceneCB()[i] = raytracer->GetSceneCB()[frameIndex];
    }
}

// Create resources that depend on the device.
void Graphics::CreateDeviceDependentResources()
{
    // Initialize raytracing pipeline.
    raytracer->CreateRaytracingInterfaces(device_resources.get());
    raytracer->CreateRootSignatures(device_resources.get());
    raytracer->CreateRaytracingPipelineStateObject();
    raytracer->CreateDescriptorHeap(device_resources.get(), scene->GetSceneModelCount("Scenes/Scene0.txt"));

    scene->LoadScene(device_resources.get(), raytracer, "Scenes/scene0.txt");
    acceleration_structure->BuildAccelerationStructures(raytracer, device_resources.get(), scene);

    raytracer->CreateConstantBuffers(device_resources.get());
    raytracer->BuildShaderTables(device_resources.get(), scene->GetScenePortals());
    raytracer->CreateRaytracingOutputResource(device_resources.get(), m_width, m_height);
}

void Graphics::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = device_resources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

// Update frame-based values.
void Graphics::OnUpdate()
{
    timer.Tick();
    CalculateFrameStats();
    float elapsedTime = static_cast<float>(timer.GetElapsedSeconds());
    auto frameIndex = device_resources->GetCurrentFrameIndex();
    auto prevFrameIndex = device_resources->GetPreviousFrameIndex();

    camera->UpdateCameraMatrices(device_resources.get(), raytracer, m_aspectRatio);

    //rotates light around level
    float time_to_rotate = 10.0f;
    float angle = -360.0f * (elapsedTime / time_to_rotate);
    float light_seconds = 8.0f;
    float time = (elapsedTime / time_to_rotate);
    XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angle));
    const XMVECTOR& prevLightPosition = raytracer->GetSceneCB()[prevFrameIndex].lightPosition;
    raytracer->GetSceneCB()[frameIndex].lightPosition = XMVector3Transform(prevLightPosition, rotate);
}

void Graphics::OnMouseMove(UINT x, UINT y) 
{
    camera->UpdateMouseXY(x, y);
    float elapsedTime = static_cast<float>(timer.GetElapsedSeconds());
    camera->UpdateMouseCameraRotation(elapsedTime);
}

void Graphics::OnKeyDown(UINT8 key)
{
    if (key == 'W')
    {
        camera->MoveForward();
    }
    if (key == 'S')
    {
        camera->MoveBackward();
    }
    if (key == 'A')
    {
        camera->MoveLeftward();
    }
    if (key == 'D')
    {
        camera->MoveRightward();
    }
}

void Graphics::UpdateForSizeChange(UINT width, UINT height)
{
    DXSample::UpdateForSizeChange(width, height);
}

void Graphics::CreateWindowSizeDependentResources()
{
    raytracer->CreateRaytracingOutputResource(device_resources.get(),m_width, m_height);
    camera->UpdateCameraMatrices(device_resources.get(), raytracer, m_aspectRatio);
}

void Graphics::ReleaseWindowSizeDependentResources()
{
    raytracer->GetRaytracingOutput().Reset();
}

void Graphics::ReleaseDeviceDependentResources()
{
    raytracer->ReleaseRaytracerResources();
    acceleration_structure->ReleaseStructures();

}

void Graphics::RecreateD3D()
{
    try
    {
        device_resources->WaitForGpu();
    }
    catch (HrException&)
    {
        // Do nothing.
    }
    device_resources->HandleDeviceLost();
}

void Graphics::OnRender()
{
    if (!device_resources->IsWindowVisible())
    {
        return;
    }

    device_resources->Prepare();
    raytracer->DoRaytracing(device_resources.get(), m_width, m_height, acceleration_structure->GetTopLevelStructure());
    raytracer->CopyRaytracingOutputToBackbuffer(device_resources.get());

    device_resources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void Graphics::OnDestroy()
{
    device_resources->WaitForGpu();
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

void Graphics::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

void Graphics::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

void Graphics::CalculateFrameStats()
{
    static int frameCnt = 0;
    static double elapsedTime = 0.0f;
    double totalTime = timer.GetTotalSeconds();
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
            << L"    GPU[" << device_resources->GetAdapterID() << L"]: " << device_resources->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

// Handle OnSizeChanged message event.
void Graphics::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!device_resources->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}