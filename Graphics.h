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
#include "GeometryData.h"
#include "FrameResource.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Graphics
{
public:
	Graphics(HWND hwnd,int width, int height, int numRenderItems);
	~Graphics();
	
	ComPtr<ID3D12Device> mD3DDevice;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;

	ComPtr<IDXGIFactory4> mDXGIFactory;
	ComPtr<IDXGISwapChain> mSwapChain;

	ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	ComPtr<ID3D12CommandAllocator> mCommandAllocator;
	ComPtr<ID3D12CommandQueue> mCommandQueue;

	const static int mSwapChainBufferCount = 2;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT mMSAAQuality = 0;
	int mMSAASampleCount = 4;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	int mCurrentBackBuffer = 0;
	ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	ComPtr<ID3D12Resource> mDepthStencilBufferGUI;

	D3D12_CPU_DESCRIPTOR_HANDLE MSAAView();
	ComPtr<ID3D12Resource> mMSAARenderTarget;

	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;

	ComPtr<ID3D12RootSignature> mRootSignature;

	int mNumRenderItems = 0;
	const static int mNumFrameResources = 3;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	ComPtr<ID3D12DescriptorHeap> mCBVHeap;
	UINT mPassCbvOffset = 0;
	UINT mGUISRVOffset = 0;

	// Compiled shader variables
	ComPtr<ID3DBlob> mVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPSByteCode = nullptr;

	XMVECTORF32 mBackgroundColour = DirectX::Colors::Purple;

	// Input layout
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	bool CreateDeviceAndFence();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hWND, int width, int height);

	void BuildFrameResources();
	void CreateConstantBuffers();
	void CreateCBVHeap();

	void Resize(int width, int height);
	void EmptyCommandQueue();

	ID3D12Resource* CurrentBackBuffer(); // Returns current back buffer in swap chain
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(); // Returns Render Target View to current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView(); // Returns Depth / Stencil View  to main depth buffer

	void ExecuteCommands();
	void CreateDescriptorHeaps();

	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);

	void CreateRootSignature();
	void CreateShaders();
	void CreatePSO();

	void ResolveMSAAToBackBuffer(ID3D12GraphicsCommandList* commandList);
	void CycleFrameResources();
};



