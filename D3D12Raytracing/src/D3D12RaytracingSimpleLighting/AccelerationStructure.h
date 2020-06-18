#pragma once
#include "stdafx.h"
class Raytracer;
class AccelerationStructure
{
public:
	AccelerationStructure();
	~AccelerationStructure();
	void BuildAccelerationStructures(DX::DeviceResources* device_resources, Raytracer* raytracer);
	void BuildGeometryDescs(Raytracer* raytracer, std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs, std::vector<int> index_offsets, std::vector<int> vertex_offsets);
	void ReleaseStructures();
	ComPtr<ID3D12Resource> GetTopLevelStructure() { return m_topLevelAccelerationStructure; }
private:
	// Acceleration structure
	ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;
};