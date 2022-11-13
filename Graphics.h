#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "d3dx12.h"
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>
#include "Utility.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Graphics
{
public:
	Graphics(HWND hwnd,int width, int height);
	~Graphics();

	ComPtr<ID3D12Device> mD3DDevice;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;

	ComPtr<IDXGIFactory4> mDXGIFactory;
	ComPtr<IDXGISwapChain> mSwapChain;

	ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12CommandAllocator> mCommandAllocator;

	const static int mSwapChainBufferCount = 2;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	int mCurrentBackBuffer = 0;
	ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;

	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;

	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mViewMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mProjectionMatrix = MakeIdentity4x4();

	bool InitD3D();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hWND, int width, int height);
	void OnResize(int width, int height);
	void FlushCommandQueue();
	ID3D12Resource* CurrentBackBuffer(); // Returns current back buffer in swap chain
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(); // Returns Render Target View to current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView(); // Returns Depth / Stencil View  to main depth buffer
	XMFLOAT4X4 MakeIdentity4x4();
};


