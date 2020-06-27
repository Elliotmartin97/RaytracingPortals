using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;

public class SaveSceneToFile : MonoBehaviour
{
    private List<GameObject> Models;
    public bool generate = false;
    public string scene_name = "scene0";

    private void AddGameobjectsToList()
    {
        Models = new List<GameObject>();
        int count = transform.childCount;
        for(int i = 0; i < count; i++)
        {
            Models.Add(transform.GetChild(i).gameObject);
        }
    }

    private void Update()
    {
        if(generate)
        {
            AddGameobjectsToList();
            GenerateFileFromModelList();
            generate = false;
            Debug.Log(Models[1].transform.position);
        }
    }

    private void GenerateFileFromModelList()
    {
        if(File.Exists("Assets/CreatedScenes/" + scene_name + ".txt"))
        {
            Debug.Log("Scene already exists with that name");
            return;

        }
        StreamWriter sr = File.CreateText("Assets/CreatedScenes/" + scene_name + ".txt");
        sr.WriteLine("# Scene file");
        sr.WriteLine("# Scene Name: " + scene_name);
        sr.WriteLine("# Format: Model name, PositionXYZ, RotationXYZ, ScaleXYZ");
        for(int i = 0; i < Models.Count; i++)
        {
            sr.WriteLine(Models[i].name + " " +
                         Models[i].transform.position.x + " " +
                         Models[i].transform.position.y + " " +
                         Models[i].transform.position.z + " " +
                         Models[i].transform.rotation.eulerAngles.x + " " +
                         Models[i].transform.rotation.eulerAngles.y + " " +
                         Models[i].transform.rotation.eulerAngles.z + " " +
                         Models[i].transform.localScale.x + " " +
                         Models[i].transform.localScale.y + " " +
                         Models[i].transform.localScale.z + " ");
        }
        sr.Close();
    }
}
