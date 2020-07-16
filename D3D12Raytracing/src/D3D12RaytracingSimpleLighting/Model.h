#pragma once
#include "stdafx.h"
#include "DirectXRaytracingHelper.h"
#include "RayTracingHlslCompat.h"
#include <vector>

class Raytracer;
class Scene;
class Model
{
public:
	Model();
	~Model();
	void LoadModelFromOBJ(std::string file_name);
	void LoadModelFromPLY(std::string file_name, std::vector<Index>& scene_indices, std::vector<Vertex>& scene_vertices, std::vector<int> &index_counts,
		std::vector<int> &vertex_counts, std::vector<int> &index_start_positions, std::vector<int> &vertex_start_positions);
	void BuildGeometryBuffers(DX::DeviceResources* device_resources, Raytracer* raytracer, Scene* scene, int buffer_index);
	std::vector<Index> GetIndex() { return model_indices; }
	std::vector<Vertex> GetVertex() { return model_vertices; }
	XMVECTOR GetPosition() { return position; }
	XMVECTOR GetRotation() { return rotation; }
	XMVECTOR GetScale() { return scale; }
	void SetModelName(std::string name);
	std::string GetModelName() { return model_name; }
	void SetPosition(XMVECTOR pos) { position = pos; }
	void SetRotation(XMVECTOR rot) { rotation = rot; }
	void SetScale(XMVECTOR sca) { scale = sca; }
private:
	std::string model_name;
	std::vector<Index> model_indices;
	std::vector<Vertex> model_vertices;
	XMVECTOR position;
	XMVECTOR rotation;
	XMVECTOR scale;
};
