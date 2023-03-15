#pragma once

#include "FrameResource.h"
#include "SRVDescriptorHeap.h"
#include <vector>
#include <memory>
extern std::vector<std::unique_ptr<FrameResource>> FrameResources;
extern int CurrentFrameResourceIndex;
extern unique_ptr<SRVDescriptorHeap> SrvDescriptorHeap;
extern UINT CbvSrvUavDescriptorSize;