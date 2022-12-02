#include "UploadBuffer.h"
#include <d3d12.h>
#include "d3dx12.h"
#include <memory>
#include "Utility.h"

class FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT planetMaxVertexCount, UINT planetMaxIndexCount);

	ComPtr<ID3D12CommandAllocator> mCommandAllocator;

	struct mPerObjectConstants
	{
		XMFLOAT4X4 WorldMatrix;
	};
	std::unique_ptr <UploadBuffer<mPerObjectConstants>> mPerObjectConstantBuffer;

    struct mPerFrameConstants
    {
        XMFLOAT4X4 ViewMatrix = MakeIdentity4x4();
        XMFLOAT4X4 ProjMatrix = MakeIdentity4x4();
        XMFLOAT4X4 ViewProjMatrix = MakeIdentity4x4();
        XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
        float padding1 = 0.0f;
        XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
        float NearZ = 0.0f;
        float FarZ = 0.0f;
        float TotalTime = 0.0f;
        float DeltaTime = 0.0f;
        float padding2 = 0.0f;
        float padding3 = 0.0f;
    };
    std::unique_ptr <UploadBuffer<mPerFrameConstants>> mPerFrameConstantBuffer;

    std::unique_ptr <UploadBuffer<Vertex>> mPlanetVB;
    std::unique_ptr <UploadBuffer<uint32_t>> mPlanetIB;

    UINT64 Fence = 0;
private:

};