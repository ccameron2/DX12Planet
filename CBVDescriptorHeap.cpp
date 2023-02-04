#include "CBVDescriptorHeap.h"

CBVDescriptorHeap::CBVDescriptorHeap(ID3D12Device* device, std::vector<std::unique_ptr<FrameResource>> const& frameResources, UINT numRenderItems, UINT cbvDescriptorSize)
{
	// Create CBV Heap
	UINT objCount = (UINT)numRenderItems;

	UINT numFrameResources = frameResources.size();

	// Need a CBV descriptor for each object for each frame resource,
	UINT numDescriptors = (objCount + 1) * numFrameResources; // +1 for the perFrameCB for each frame resource.

	// Save an offset to the start of the per frame CBVs.
	mFrameCbvOffset = objCount * numFrameResources;

	// Save an offset to the start of the GUI SRVs
	mGuiSrvOffset = (objCount * numFrameResources) + numFrameResources;

	mCBVDescriptorSize = cbvDescriptorSize;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors + 1; // For GUI
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	if (FAILED(device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCBVHeap))))
	{
		MessageBox(0, L"Create descriptor heap failed", L"Error", MB_OK);
	}

	// Create constant buffers
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));
	UINT numObjects = (UINT)numRenderItems;
	
	for (int frameIndex = 0; frameIndex < numFrameResources; frameIndex++)
	{
		auto objectConstantBuffer = frameResources[frameIndex]->mPerObjectConstantBuffer->GetBuffer();
		for (UINT i = 0; i < numObjects; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectConstantBuffer->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * numObjects + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, cbvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	UINT passCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerFrameConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < frameResources.size(); frameIndex++)
	{
		auto passCB = frameResources[frameIndex]->mPerFrameConstantBuffer->GetBuffer();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mFrameCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, cbvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		device->CreateConstantBufferView(&cbvDesc, handle);
	}
}

CBVDescriptorHeap::~CBVDescriptorHeap()
{
}
