#pragma once

#include <d3d12.h>
#include "d3dx12.h"
#include <wrl.h>
#include "Utility.h"
#include "FrameResource.h"
#include <vector>
#include <memory>

using Microsoft::WRL::ComPtr;

class DescriptorHeap
{
public:
	DescriptorHeap(ID3D12Device* device, std::vector<std::unique_ptr<FrameResource>> const& frameResources, UINT numRenderItems, UINT cbvSrvUavDescriptorSize);
	~DescriptorHeap();
	ComPtr<ID3D12DescriptorHeap> mCBVHeap;
	UINT mFrameCbvOffset = 0;
	UINT mGuiSrvOffset = 0;
};

