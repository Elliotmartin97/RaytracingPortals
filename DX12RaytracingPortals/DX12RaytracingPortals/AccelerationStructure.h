#pragma once
#include "stdafx.h"
#include "WindowsApplication.h"
class AccelerationStructure
{
public:

private:

	ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

};