using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayerCamera : MonoBehaviour
{
    public float speed = 5.0f;
    public float look_sensitivity = 100.0f;
    private float x_rotation;
    private float y_rotation;
    public List<PortalScript> portals;
    // Start is called before the first frame update

    private void OnPostRender()
    {
        for(int i = 0; i < portals.Count; i++)
        {
            portals[i].Render();
        }
    }

    private void Update()
    {
        Debug.DrawRay(transform.position, transform.forward * 10, Color.green);
        float mouseX = Input.GetAxis("Mouse X") * look_sensitivity * Time.deltaTime;
        float mouseY = Input.GetAxis("Mouse Y") * look_sensitivity * Time.deltaTime;
        x_rotation -= mouseY;
        y_rotation += mouseX;
        transform.localRotation = Quaternion.Euler(x_rotation, y_rotation, 0f);

        if (Input.GetKey(KeyCode.W))
        {
            transform.Translate(Vector3.forward  * speed * Time.deltaTime);
        }
        if (Input.GetKey(KeyCode.S))
        {
            transform.Translate(Vector3.forward * -speed * Time.deltaTime);
        }
        if (Input.GetKey(KeyCode.A))
        {
            transform.Translate(Vector3.right * -speed * Time.deltaTime);
        }
        if (Input.GetKey(KeyCode.D))
        {
            transform.Translate(Vector3.right * speed * Time.deltaTime);
        }
    }
}
