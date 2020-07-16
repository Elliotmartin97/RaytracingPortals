#pragma once
#include "stdafx.h"

using namespace DirectX;

class Raytracer;

class Camera
{
public:
    void UpdateCameraMatrices(DX::DeviceResources* device_resources, Raytracer* raytracer, float aspect_ratio);

    void MoveForward();
    void MoveBackward();
    void MoveRightward();
    void MoveLeftward();

    XMVECTOR GetPositionVector() { return position_vector; }
    XMFLOAT3 GetPositionFloat() { return position_float; }
    XMVECTOR GetRotationVector() { return rotation_vector; }
    XMFLOAT3 GetRotationFloat() { return rotation_float; }

    void SetPositionVector(XMVECTOR vector);
    void SetPositionFloat(float x, float y, float z);
    void SetRotationVector(XMVECTOR vector);
    void SetRotationFloat(float x, float y, float z);
    void SetRotationFloatRadians(float x, float y, float z);

    void AddPositionVector(XMVECTOR vector);
    void AddPositionFloat(float x, float y, float z);
    void AddRotationVector(XMVECTOR vector);
    void AddRotationFloat(float x, float y, float z);
    void AddRotationFloatRadians(float x, float y, float z);

    void SetMouseCenterPosition(int x, int y);
    void UpdateMouseXY(int x, int y);
    void UpdateMouseCameraRotation(float elapsed_time);
    void SetProjectionMatrix(float fov, float aspect_ratio, float near_z, float far_z);
    void UpdateViewMatrix();
    XMMATRIX GetProjectionMatrix() { return projection_matrix; }
    XMMATRIX GetViewMatrix() { return view_matrix; }

private:

    XMVECTOR position_vector;
    XMFLOAT3 position_float;
    XMVECTOR rotation_vector;
    XMFLOAT3 rotation_float;
    XMVECTOR camera_forward;
    XMVECTOR camera_right;
    XMVECTOR camera_up;

    XMMATRIX view_matrix;
    XMMATRIX projection_matrix;

    float mouse_x = 0;
    float mouse_y = 0;
    float center_x = 0;
    float center_y = 0;
    float pitch = 0;
    float yaw = 0;

    const XMVECTOR FORWARD_VECTOR = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const XMVECTOR UP_VECTOR = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR RIGHT_VECTOR = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
};