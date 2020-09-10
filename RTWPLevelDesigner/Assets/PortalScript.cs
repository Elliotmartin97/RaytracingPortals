using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PortalScript : MonoBehaviour
{
    public PortalScript linked_portal;
    public MeshRenderer screen;
    public Camera player_cam;
    private Camera portal_cam;
    private RenderTexture view_texture;

    private void Awake()
    {
        player_cam = GameObject.Find("PlayerCamera").GetComponent<Camera>();
        portal_cam = GetComponentInChildren<Camera>();
        portal_cam.enabled = false;
    }
    private void Update()
    {
        Debug.DrawRay(portal_cam.transform.position, portal_cam.transform.forward * 10, Color.red);
    }

    void CreateViewTexture()
    {
        if(view_texture == null || view_texture.width != Screen.width || view_texture.height != Screen.height)
        {
            if(view_texture != null)
            {
                view_texture.Release();
            }

            view_texture = new RenderTexture(Screen.width, Screen.height, 0);
            portal_cam.targetTexture = view_texture;
            linked_portal.screen.material.SetTexture("_MainTex", view_texture);
        }
    }

    public void Render()
    {
        screen.enabled = false;
        CreateViewTexture();

        Matrix4x4 portal_matrix = transform.localToWorldMatrix * linked_portal.transform.worldToLocalMatrix * player_cam.transform.localToWorldMatrix;
        portal_cam.transform.SetPositionAndRotation(portal_matrix.GetColumn(3), portal_matrix.rotation);
        portal_cam.Render();
        Debug.Log(transform.name + " Rendered!");
        screen.enabled = true;
    }
}
