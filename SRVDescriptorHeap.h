#pragma once

#include <d3d12.h>
#include "d3dx12.h"
#include <wrl.h>
#include "Utility.h"
#include "FrameResource.h"
#include <vector>
#include <memory>

using Microsoft::WRL::ComPtr;

class SRVDescriptorHeap
{
public:
	SRVDescriptorHeap(ID3D12Device* device, UINT cbvDescriptorSize);
	~SRVDescriptorHeap();
	ComPtr<ID3D12DescriptorHeap> mHeap;
	UINT mGuiSrvOffset = 0;
	UINT mDescriptorSize = 0;
	int mMaxTextures = 60;
	int mCurrentIndex = 0;
};

