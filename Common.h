#pragma once

#include "FrameResource.h"
#include "SRVDescriptorHeap.h"
#include <vector>
#include <memory>

const int MAX_PLANET_VERTS = 250000;

extern std::vector<std::unique_ptr<FrameResource>> FrameResources;
extern int CurrentFrameResourceIndex;
extern unique_ptr<SRVDescriptorHeap> SrvDescriptorHeap;
extern UINT CbvSrvUavDescriptorSize;
extern ComPtr<ID3D12CommandQueue> CommandQueue;
extern ComPtr<ID3D12Device> D3DDevice;
extern int CurrentSRVOffset;
