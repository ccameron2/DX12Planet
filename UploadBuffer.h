#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include "Utility.h"

using Microsoft::WRL::ComPtr;

template <typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool constant)
	{
		mElementSize = sizeof(T);
		if (constant)
		{
			mElementSize = CalculateConstantBufferSize(sizeof(T));
		}

		if(FAILED(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mElementSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer))));

		if (FAILED(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mData))))
		{
			MessageBox(0, L"Buffer map failed", L"Error", MB_OK);
		}

	}

	~UploadBuffer()
	{
		if (mUploadBuffer) { mUploadBuffer->Unmap(0, nullptr); mData = nullptr; }
	}

	void Copy(int element, const T& data)
	{
		memcpy(&mData[element * mElementSize], &data, sizeof(T));
	}

	ID3D12Resource* GetBuffer() { return mUploadBuffer.Get(); }

private:
	ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mData = nullptr;
	UINT mElementSize = 0;
};

