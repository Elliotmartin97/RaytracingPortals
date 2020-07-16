#include "stdafx.h"
#include "Model.h"
#include "DirectXRaytracingHelper.h"
#include "RayTracingHlslCompat.h"
#include "Raytracer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include "RayTracingHlslCompat.h"
#include "Scene.h"

/*
Model loading: Read in the PLY vertex, normal and index grouping values
Create new vertex and normal values using the grouping values
(New vertex for each time the same one is used but with a different normal)
OBJ LOADER NOT READY USE PLY INSTEAD
*/

Model::Model() 
{
}
Model::~Model()
{
}


void Model::LoadModelFromOBJ(std::string file_name)
{
    

    std::string line;
    std::ifstream file;
    file.open(file_name);
    char type;
    std::vector<int> obj_normal_indexes;
    std::vector<UINT16> obj_vertex_indexes;
    std::vector<XMFLOAT3> obj_normals;
    std::vector<XMFLOAT3> obj_vertices;

    if (file.is_open())
    {
        int vert_count = 0;
        int normal_count = 0;
        int face_count = 0;
        while (std::getline(file, line))
        {
            if (line[0] == 'v' && line[1] == ' ')
            {
                float v1, v2, v3;
                std::istringstream iss(line.substr(1));
                iss >> v1 >> v2 >> v3;
                XMFLOAT3 xm_vertex = XMFLOAT3(v1, v2, v3);
                obj_vertices.push_back(xm_vertex);
                vert_count += 3;
            }
            else if (line[0] == 'v' && line[1] == 'n')
            {
                float n1, n2, n3;
                std::istringstream iss(line.substr(2));
                iss >> n1 >> n2 >> n3;
                XMFLOAT3 xm_normal = XMFLOAT3(n1, n2, n3);
                obj_normals.push_back(xm_normal);
            }
            else if (line[0] == 'f' && line[1] == ' ')
            {
                for (int i = 0; i < line.size(); i++)
                {
                    if (line[i] == '/')
                    {
                        line[i] = ' ';
                    }
                }
                int current_char = 0;
                int ch;
                std::istringstream iss(line.substr(2));
                obj_vertex_indexes.push_back(0);
                obj_vertex_indexes.push_back(0);
                obj_vertex_indexes.push_back(0);
                obj_normal_indexes.push_back(0);
                obj_normal_indexes.push_back(0);
                obj_normal_indexes.push_back(0);
                iss >> obj_vertex_indexes[face_count];
                iss >> ch >> obj_normal_indexes[face_count];
                iss >> obj_vertex_indexes[face_count + 1];
                iss >> ch >> obj_normal_indexes[face_count + 1];
                iss >> obj_vertex_indexes[face_count + 2];
                iss >> ch >> obj_normal_indexes[face_count + 2];
                face_count += 3;
            }
        }

        //obj indexing starts at 1 instead of 0, so take away 1
        for (int i = 0; i < obj_normal_indexes.size(); i++)
        {
            obj_normal_indexes[i]--;
            obj_vertex_indexes[i]--;
        }

        std::vector<int> prev_vs;
        std::vector<int> prev_ns;
        //go through the data of the obj file, create new data to use for index and vertex buffers
        //TODO check and delete duplicates
        for (int i = 0; i < obj_vertex_indexes.size(); i++)
        {
            bool duplicate = false;
            model_indices.push_back(i);
            for (int y = 0; y < prev_vs.size(); y++)
            {
                if (obj_vertex_indexes[i] == prev_vs[y] && obj_normal_indexes[i] == prev_ns[y])
                {
                    duplicate = true;
                }
            }
            if (duplicate == true)
            {
                continue;
            }

            XMFLOAT3 new_vertex = obj_vertices[obj_vertex_indexes[i]];
            XMFLOAT3 new_normal = obj_normals[obj_normal_indexes[i]];
            Vertex new_VN = { new_vertex, new_normal };
            model_indices.push_back(obj_vertex_indexes[i]);
            model_vertices.push_back(new_VN);
            prev_vs.push_back(obj_vertex_indexes[i]);
            prev_ns.push_back(obj_normal_indexes[i]);
            
        }

        //now need to recreate indices to the new vertices list to build triangles


        //int indices_size = vertex_indexes.size();
        //indices.resize(face_count);
        //for (int i = 0; i < face_count; i++)
        //{
        //    indices[i] = vertex_indexes[i];
        //}

        //vertex_vector.resize(vert_count / 3);

        //for (int i = 0; i < vert_count / 3; i++)
        //{
        //    vertex_vector[i] = {
        //        XMFLOAT3(obj_vertices[i]),
        //        XMFLOAT3(obj_normals[4])
        //    };
        //}
    }
}

void Model::SetModelName(std::string name)
{
    model_name = name;
}

void Model::LoadModelFromPLY(std::string file_name, std::vector<Index> &scene_indices, std::vector<Vertex> &scene_vertices, std::vector<int> &index_counts,
    std::vector<int> &vertex_counts, std::vector<int> &index_start_positions, std::vector<int> &vertex_start_positions)
{
    std::string line;
    std::ifstream file;
    file.open("Models/" + file_name + ".ply");

    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            if (isdigit(line[0]) || line[0] == '-')
            {
                if (line[0] == '3')
                {
                    std::istringstream iss(line.substr(2));
                    int i1, i2, i3;
                    iss >> i1 >> i2 >> i3;
                    model_indices.push_back(i1);
                    model_indices.push_back(i2);
                    model_indices.push_back(i3);
                }
                else
                {
                    std::istringstream iss(line);

                    float vx, vy, vz, nx, ny, nz;
                    iss >> vx >> vy >> vz >> nx >> ny >> nz;
                    XMFLOAT3 xm_vert(vx, vy, vz);
                    XMFLOAT3 xm_norm(nx, ny, nz);
                    Vertex new_VN = { xm_vert,xm_norm };
                    model_vertices.push_back(new_VN);
                }
            }
        }
    }
    file.close();

    scene_indices.insert(scene_indices.end(), model_indices.begin(), model_indices.end());
    scene_vertices.insert(scene_vertices.end(), model_vertices.begin(), model_vertices.end());
    index_counts.push_back(model_indices.size());
    vertex_counts.push_back(model_vertices.size());

    int previous_index_total = scene_indices.size() - model_indices.size();
    int previous_vertex_total = scene_vertices.size() - model_vertices.size();
    index_start_positions.push_back(previous_index_total);
    vertex_start_positions.push_back(previous_vertex_total);
    
}

void Model::BuildGeometryBuffers(DX::DeviceResources* device_resources, Raytracer* raytracer, Scene* scene, int b_idx)
{
    auto device = device_resources->GetD3DDevice();
    //// Cube indices.
    int i_count = model_indices.size();
    int v_count = model_vertices.size();
    raytracer->AddBufferSlot();
    AllocateUploadBuffer(device, &model_indices[0], i_count * sizeof(int), &raytracer->GetIndexBufferByIndex(b_idx)->resource);
    AllocateUploadBuffer(device, &model_vertices[0], v_count * sizeof(Vertex), &raytracer->GetVertexBufferByIndex(b_idx)->resource);

    // Vertex buffer is passed to the shader along with index buffer as a descriptor table.
    // Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
    UINT descriptorIndexIB = raytracer->CreateBufferSRV(device_resources, raytracer->GetIndexBufferByIndex(b_idx), (i_count * 2) / 4, 0);
    UINT descriptorIndexVB = raytracer->CreateBufferSRV(device_resources, raytracer->GetVertexBufferByIndex(b_idx), v_count, sizeof(XMFLOAT3) * 2);
    ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
}