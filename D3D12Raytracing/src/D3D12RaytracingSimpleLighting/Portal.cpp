#include "stdafx.h"
#include "Portal.h"
#include "Model.h"

Portal::Portal()
{
	portal_model = new Model();
}

Portal::~Portal()
{
	portal_model = nullptr;
}

void Portal::SetPortalModel(Model& model)
{
	portal_model = &model;
}

void Portal::LoadPortalInScene()
{
	
}
