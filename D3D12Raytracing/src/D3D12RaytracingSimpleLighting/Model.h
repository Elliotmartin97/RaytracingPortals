#pragma once
#include "stdafx.h"
#include "DirectXRaytracingHelper.h"
#include "RayTracingHlslCompat.h"
#include <vector>

class Raytracer;

class Model
{
public:
	Model();
	~Model();
	void BuildGeometry(DX::DeviceResources* device_resources, Raytracer* raytracer);
	void LoadModelFromOBJ(std::string file_name);
	void LoadModelFromPLY(std::string file_name);
	std::vector<Index> GetIndex() { return model_indices; }
	std::vector<Vertex> GetVertex() { return model_vertices; }
private:
	std::vector<Index> model_indices;
	std::vector<Vertex> model_vertices;
};