#pragma once
#include "stdafx.h"
#include "Model.h"
class Scene
{
public:
    Scene() = default;
    ~Scene() = default;
	void LoadScene(std::string filename);
	std::vector<Model> GetSceneModels() { return scene_models; }
    std::vector<Index> GetSceneIndices() { return scene_indices; }
    std::vector<Vertex> GetSceneVertices() { return scene_vertices; }
    std::vector<int> GetIndexCounts() { return index_counts; }
    std::vector<int> GetVertexCounts() { return vertex_counts; }
    std::vector<int> GetIndexLocations() { return index_locations; }
    std::vector<int> GetVertexLocations() { return vertex_locations; }
private:
	std::vector<Model> scene_models;
    std::vector<Index> scene_indices;
    std::vector<Vertex> scene_vertices;
    std::vector<int> index_counts;
    std::vector<int> vertex_counts;
    std::vector<int> index_locations;
    std::vector<int> vertex_locations;
};