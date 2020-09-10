using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class RotatorLight : MonoBehaviour
{
    // Start is called before the first frame update
    public GameObject point;
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        transform.RotateAround(point.transform.position, Vector3.up, -50 * Time.deltaTime);
    }
}
