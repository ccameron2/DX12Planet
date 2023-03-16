#include "SRVDescriptorHeap.h"

SRVDescriptorHeap::SRVDescriptorHeap(ID3D12Device* device, UINT cbvDescriptorSize)
{
	// Create the SRV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mMaxTextures;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mHeap));

	mGuiSrvOffset = 0;
	mDescriptorSize = cbvDescriptorSize;
}

SRVDescriptorHeap::~SRVDescriptorHeap()
{
}

