#include "stdafx.h"
#include "Camera.h"
#include "Raytracer.h"

//void Camera::SetupCamera()
//{
//    m_curRotationAngleRad = 45.0f;
//    XMVECTOR direction = XMVector4Normalize(m_at - m_eye);
//    m_up = XMVector3Normalize(XMVector3Cross(direction, RIGHT_VECTOR));
//}

void Camera::UpdateCameraMatrices(DX::DeviceResources* device_resources, Raytracer* raytracer, float aspect_ratio)
{
    auto frameIndex = device_resources->GetCurrentFrameIndex();

    raytracer->GetSceneCB()[frameIndex].cameraPosition = position_vector;

    XMMATRIX viewProj = view_matrix * projection_matrix;

    raytracer->GetSceneCB()[frameIndex].projectionToWorld = XMMatrixInverse(nullptr, viewProj);
}

void Camera::SetProjectionMatrix(float fov, float aspect_ratio, float near_z, float far_z)
{
    float fov_radians = (fov / 360.0f) * XM_2PI;
    projection_matrix = XMMatrixPerspectiveFovLH(fov_radians, aspect_ratio, near_z, far_z);
}

void Camera::UpdateViewMatrix()
{
    XMMATRIX camera_rotation = XMMatrixRotationRollPitchYaw(rotation_float.x, rotation_float.y, rotation_float.z);
    XMVECTOR camera_target = XMVector3TransformCoord(FORWARD_VECTOR, camera_rotation);
    XMVECTOR target_right = XMVector3TransformCoord(RIGHT_VECTOR, camera_rotation);
    camera_right = XMVector3Normalize(target_right);
    camera_forward = XMVector3Normalize(camera_target);
    camera_target += position_vector;
    XMVECTOR camera_up = XMVector3TransformCoord(UP_VECTOR, camera_rotation);
    view_matrix = XMMatrixLookAtLH(position_vector, camera_target, camera_up);
}

void Camera::MoveForward()
{
    position_vector += camera_forward * 0.1f;
    UpdateViewMatrix();
}

void Camera::MoveBackward()
{
    position_vector -= camera_forward * 0.1f;
    UpdateViewMatrix();
}

void Camera::MoveRightward()
{
    position_vector += camera_right * 0.1f;
    UpdateViewMatrix();
}

void Camera::MoveLeftward()
{
    position_vector -= camera_right * 0.1f;
    UpdateViewMatrix();
}

void Camera::UpdateMouseXY(int x, int y)
{
    mouse_x = x;
    mouse_y = y;
}

void Camera::SetMouseCenterPosition(int x, int y)
{
    center_x = x;
    center_y = y;
}

void Camera::UpdateMouseCameraRotation(float elapsed_time)
{
    if (mouse_x == center_x && mouse_y == center_y)
    {
        return;
    }
    else
    {
        float x_difference = mouse_x - center_x;
        float y_difference = mouse_y - center_y;
        pitch += (x_difference * 0.01f);
        yaw += (y_difference * 0.01f);
        SetRotationFloat(yaw, pitch, 0.0f);
        UpdateViewMatrix();
    }
}

// POSITION
void Camera::SetPositionVector(XMVECTOR pos)
{
    position_vector = pos;
    XMStoreFloat3(&position_float, position_vector);
    UpdateViewMatrix();
}

void Camera::SetPositionFloat(float x, float y, float z)
{
    XMFLOAT3 pos = { x, y, z };
    position_float = pos;
    position_vector = XMLoadFloat3(&position_float);
    UpdateViewMatrix();
}

void Camera::AddPositionVector(XMVECTOR pos)
{
    position_vector += pos;
    XMStoreFloat3(&position_float, position_vector);
    UpdateViewMatrix();
}

void Camera::AddPositionFloat(float x, float y, float z)
{
    position_float.x += XMConvertToRadians(x);
    position_float.y += XMConvertToRadians(y);
    position_float.z += XMConvertToRadians(z);
    position_vector = XMLoadFloat3(&position_float);
    UpdateViewMatrix();
}

// ROTATION
void Camera::SetRotationVector(XMVECTOR rot)
{
    rotation_vector = rot;
    XMStoreFloat3(&rotation_float, rotation_vector);
    UpdateViewMatrix();
}

void Camera::SetRotationFloat(float x, float y, float z)
{
    XMFLOAT3 rot = { XMConvertToRadians(x), XMConvertToRadians(y), XMConvertToRadians(z) };
    rotation_float = rot;
    rotation_vector = XMLoadFloat3(&rotation_float);
    UpdateViewMatrix();
}

void Camera::SetRotationFloatRadians(float x, float y, float z)
{
    XMFLOAT3 rot = { x, y, z };
    rotation_float = rot;
    rotation_vector = XMLoadFloat3(&rotation_float);
    UpdateViewMatrix();
}

void Camera::AddRotationVector(XMVECTOR rot)
{
    rotation_vector += rot;
    XMStoreFloat3(&rotation_float, rotation_vector);
    UpdateViewMatrix();
}

void Camera::AddRotationFloat(float x, float y, float z)
{
    rotation_float.x += XMConvertToRadians(x);
    rotation_float.y += XMConvertToRadians(y);
    rotation_float.z += XMConvertToRadians(z);
    rotation_vector = XMLoadFloat3(&rotation_float);
    UpdateViewMatrix();
}

void Camera::AddRotationFloatRadians(float x, float y, float z)
{
    rotation_float.x += x;
    rotation_float.y += y;
    rotation_float.z += z;
    rotation_vector = XMLoadFloat3(&rotation_float);
    UpdateViewMatrix();
}