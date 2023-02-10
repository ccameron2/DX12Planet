#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT planetMaxVertexCount, UINT planetMaxIndexCount)
{
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.GetAddressOf()));
	mPerFrameConstantBuffer = std::make_unique<UploadBuffer<mPerFrameConstants>>(device, passCount, true);
	mPerObjectConstantBuffer = std::make_unique<UploadBuffer<mPerObjectConstants>>(device, objectCount, true);
	mPlanetVB = std::make_unique<UploadBuffer<Vertex>>(device, planetMaxVertexCount, false);
	mPlanetIB = std::make_unique<UploadBuffer<uint32_t>>(device, planetMaxIndexCount, false);
}
