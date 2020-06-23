#pragma once
#include "stdafx.h"
class Raytracer;
class AccelerationStructure
{
public:
	AccelerationStructure();
	~AccelerationStructure();
	void BuildBottomLevelASInstanceDescs(DX::DeviceResources* device_resources, std::vector<D3D12_GPU_VIRTUAL_ADDRESS> bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource);
	void BuildGeometryDescsForBottomLevelAS(Raytracer* raytracer, std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> &geometryDescs, std::vector<int> index_counts, std::vector<int> vertex_counts,
		std::vector<int> index_starts, std::vector<int> vertex_start);
	AccelerationStructureBuffers BuildBottomLevelAS(DX::DeviceResources* device_resources, Raytracer* raytracer, D3D12_RAYTRACING_GEOMETRY_DESC geometryDescs);
	AccelerationStructureBuffers BuildTopLevelAS(DX::DeviceResources* device_resources, Raytracer* raytracer,
		std::vector<int> index_counts, std::vector<int> vertex_counts, int num_blas, std::vector<AccelerationStructureBuffers> bottomLevelAS);
	void BuildAccelerationStructures(Raytracer* raytracer, DX::DeviceResources* device_resources, std::vector<int> index_counts, std::vector<int> vertex_counts,
		std::vector<int> index_starts, std::vector<int> vertex_starts);

	
	void ReleaseStructures();
	ComPtr<ID3D12Resource> GetTopLevelStructure() { return m_topLevelAccelerationStructure; }
private:
	// Acceleration structure
	std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;
};