using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class raytest : MonoBehaviour
{
    public GameObject linked_portal;
    Vector3 hit_pos;
    Vector3 link_pos;
    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        Ray ray = Camera.main.ScreenPointToRay(Input.mousePosition);
        Debug.DrawRay(ray.origin, ray.direction);
    }

    private void OnMouseDown()
    {
        RaycastHit hit;
        Ray ray = Camera.main.ScreenPointToRay(Input.mousePosition);
        if(Physics.Raycast(ray, out hit, 1000))
        {
            Vector3 new_pos = hit.point - transform.position;
            hit_pos = transform.position + new_pos;
            link_pos = linked_portal.transform.position + new_pos;
            
        }
    }

    private void OnDrawGizmos()
    {
        Gizmos.DrawSphere(hit_pos, 0.05f);
        Gizmos.DrawSphere(link_pos, 0.05f);
    }
}
