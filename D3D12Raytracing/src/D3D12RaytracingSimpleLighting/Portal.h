#pragma once
#include "stdafx.h"
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
private:
	Model* portal_model;
	Portal* linked_portal;
};