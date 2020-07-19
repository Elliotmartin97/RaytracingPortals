#pragma once
#include "stdafx.h"
#include "RayTracingHlslCompat.h"
class Model;

class Portal
{
public:
	Portal();
	~Portal();
	void LoadPortalInScene();
	void SetLinkedPortal(Portal* portal);
	Model* GetModel() { return portal_model; }
	void SetPortalModel(Model& model);
	void SetPortalOrigin(XMFLOAT3 origin) { portal_origin = origin; }
	XMFLOAT3 GetPortalOrigin() { return portal_origin; }
private:
	Model* portal_model;
	Portal* linked_portal;
	XMFLOAT3 portal_origin;
};