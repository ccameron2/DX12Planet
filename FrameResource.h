
#pragma once

#include "UploadBuffer.h"
#include <d3d12.h>
#include "d3dx12.h"
#include <memory>


class FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT planetMaxVertexCount, UINT planetMaxIndexCount, UINT materialCount);

	ComPtr<ID3D12CommandAllocator> mCommandAllocator;

    std::unique_ptr <UploadBuffer<PerObjectConstants>> mPerObjectConstantBuffer;
    std::unique_ptr <UploadBuffer<PerFrameConstants>> mPerFrameConstantBuffer;
    std::unique_ptr <UploadBuffer<PerMaterialConstants>> mPerMaterialConstantBuffer;

    std::unique_ptr <UploadBuffer<Vertex>> mPlanetVB;
    std::unique_ptr <UploadBuffer<uint32_t>> mPlanetIB;

    UINT64 Fence = 0;
private:

};