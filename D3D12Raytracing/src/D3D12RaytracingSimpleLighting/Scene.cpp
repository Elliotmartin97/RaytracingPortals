#include "stdafx.h"
#include "Scene.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include "Raytracer.h"

int Scene::GetSceneModelCount(std::string filename)
{
    std::string line;
    std::ifstream file;
    std::string str;
    int model_count = 0;
    file.open(filename);
    
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            if (line[0] == '%')
            {
                std::istringstream iss(line.substr(1));

                iss >> str >> model_count;
                file.close();
                return model_count;
            }
        }
    }
    file.close();
    return 0;
}

void Scene::LoadScene(DX::DeviceResources* device_resources, Raytracer* raytracer, std::string filename)
{
    std::string line;
    std::ifstream file;

    file.open(filename);
    if (file.is_open())
    {
        std::string model_filename;
        float posx, posy, posz, rotx, roty, rotz, scax, scay, scaz;
        int model_count = 0;
        while (std::getline(file, line))
        {
            if (line[0] != '#' && line[0] != '%')
            {
                std::istringstream iss(line);
                std::string model_filename;
                iss >> model_filename >> posx >> posy >> posz >> rotx >> roty >> rotz >> scax >> scay >> scaz;
                Model model;
                model.LoadModelFromPLY(model_filename, scene_indices, scene_vertices, index_counts, vertex_counts, index_locations, vertex_locations);
                XMFLOAT3 position_float3 = XMFLOAT3(posx, posy, posz);
                XMFLOAT3 rotation_float3 = XMFLOAT3(XMConvertToRadians(rotx), XMConvertToRadians(roty), XMConvertToRadians(rotz));
                XMFLOAT3 scale_float3 = XMFLOAT3(scax, scay, scaz);
                XMVECTOR position_vector = XMLoadFloat3(&position_float3);
                XMVECTOR rotation_vector = XMLoadFloat3(&rotation_float3);
                XMVECTOR scale_vector = XMLoadFloat3(&scale_float3);
                model.SetPosition(position_vector);
                model.SetRotation(rotation_vector);
                model.SetScale(scale_vector);
                model.BuildGeometryBuffers(device_resources, raytracer, this, model_count);
                scene_models.push_back(model);
                model_count++;
            }
        }
    }

    file.close();
}