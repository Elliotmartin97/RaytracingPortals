#include "stdafx.h"
#include "Camera.h"
#include "Raytracer.h"

void Camera::SetupCamera()
{
    m_curRotationAngleRad = 45.0f;
    XMVECTOR direction = XMVector4Normalize(m_at - m_eye);
    m_up = XMVector3Normalize(XMVector3Cross(direction, right));
}

void Camera::UpdateCameraMatrices(DX::DeviceResources* device_resources, Raytracer* raytracer, float aspect_ratio)
{
    auto frameIndex = device_resources->GetCurrentFrameIndex();

    raytracer->GetSceneCB()[frameIndex].cameraPosition = m_eye;
    float fovAngleY = 60.0f;
    XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), aspect_ratio, 1.0f, 125.0f);
    XMMATRIX viewProj = view * proj;

    raytracer->GetSceneCB()[frameIndex].projectionToWorld = XMMatrixInverse(nullptr, viewProj);
}

void Camera::SetPosition(float x, float y, float z, float w)
{
    m_eye = { x, y, z, w };
}

void Camera::SetTarget(float x, float y, float z, float w)
{
    m_at = { x, y, z, w };
}

void Camera::SetUp(float x, float y, float z, float w)
{
    m_up = { x, y, z, w };
}

void Camera::RotatePosition(XMMATRIX matrix_rotation)
{
    m_eye = XMVector3Transform(m_eye, matrix_rotation);
}

void Camera::RotateTarget(XMMATRIX matrix_rotation)
{
    m_at = XMVector3Transform(m_up, matrix_rotation);
}

void Camera::RotateUp(XMMATRIX matrix_rotation)
{
    m_up = XMVector3Transform(m_up, matrix_rotation);
}