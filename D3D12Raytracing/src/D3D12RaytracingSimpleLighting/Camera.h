#pragma once
#include "stdafx.h"

using namespace DirectX;

class Raytracer;

class Camera
{
public:
    void SetupCamera();
    void UpdateCameraMatrices(DX::DeviceResources* device_resources, Raytracer* raytracer, float aspect_ratio);
    void SetUp(float x, float y, float z, float w);
    void SetPosition(float x, float y, float z, float w);
    void SetTarget(float x, float y, float z, float w);
    void RotatePosition(XMMATRIX matrix_rotation);
    void RotateTarget(XMMATRIX matrix_rotation);
    void RotateUp(XMMATRIX matrix_rotation);

private:
    float m_curRotationAngleRad;
    XMVECTOR m_eye;
    XMVECTOR m_at;
    XMVECTOR m_up;
    const XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };
};