#pragma once
#include "Common.h"
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
#include "Mesh.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Graphics
{
public:
	Graphics(HWND hwnd,int width, int height);
	~Graphics();

	static const unsigned int mNumFrameResources = 3;

	// Base command objects
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	ComPtr<ID3D12CommandAllocator> mBaseCommandAllocators[mNumFrameResources];
	
	ComPtr<ID3D12Fence1> mFence;
	UINT64 mCurrentFence = 0;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Descriptor sizes
	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;

	// Buffers
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	ComPtr<ID3D12Resource> mDepthStencilBufferGUI;

	// Heaps
	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;
	
	ComPtr<ID3D12RootSignature> mRootSignature;
	
	// Frame resource currently in use
	FrameResource* mCurrentFrameResource = nullptr;

	ComPtr<ID3D12PipelineState> mSolidPSO = nullptr;
	ComPtr<ID3D12PipelineState> mWireframePSO = nullptr;
	ComPtr<ID3D12PipelineState> mTexPSO = nullptr;
	ComPtr<ID3D12PipelineState> mSimpleTexPSO = nullptr;
	ComPtr<ID3D12PipelineState> mSkyPSO = nullptr;
	ComPtr<ID3D12PipelineState> mPlanetPSO = nullptr;
	ComPtr<ID3D12PipelineState> mWaterPSO = nullptr;

	ComPtr<ID3DBlob> mColourVSByteCode = nullptr;
	ComPtr<ID3DBlob> mColourPSByteCode = nullptr;
	ComPtr<ID3DBlob> mTexVSByteCode = nullptr;
	ComPtr<ID3DBlob> mTexPSByteCode = nullptr;
	ComPtr<ID3DBlob> mSimpleTexVSByteCode = nullptr;
	ComPtr<ID3DBlob> mSimpleTexPSByteCode = nullptr;
	ComPtr<ID3DBlob> mPlanetVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPlanetPSByteCode = nullptr;
	ComPtr<ID3DBlob> mSkyVSByteCode = nullptr;
	ComPtr<ID3DBlob> mSkyPSByteCode = nullptr;
	ComPtr<ID3DBlob> mWaterVSByteCode = nullptr;
	ComPtr<ID3DBlob> mWaterPSByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mColourInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTexInputLayout;

	D3D12_RENDER_TARGET_BLEND_DESC mTransparencyBlendDesc;
	
	// Accessor functions
	int GetBackbufferWidth() { return mBackbufferWidth; }
	int GetBackbufferHeight() { return mBackbufferHeight; }

	// Get view of MSAA buffer
	D3D12_CPU_DESCRIPTOR_HANDLE MSAAView();
	
	// Compile shader passed into constructor
	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);

	// Resize render objects
	void Resize(int width, int height);
	
	void EmptyCommandQueue();

	// Reset the command allocator for this thread
	void ResetCommandAllocator(int thread);

	// Reset base command allocator
	void ResetCommandAllocator(ID3D12CommandAllocator* commandAllocator);

	// Start a command list for this thread
	ID3D12GraphicsCommandList* StartCommandList(int thread, int list);

	// Reset the base command list
	void ResetCommandList(ID3D12CommandAllocator* commandAllocator, ID3D12PipelineState* pipeline);

	ID3D12Resource* CurrentBackBuffer(); // Returns current back buffer in swap chain
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(); // Returns Render Target View to current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView(); // Returns Depth / Stencil View  to main depth buffer

	// Functions used in Draw function
	void SetViewportAndScissorRects(ID3D12GraphicsCommandList* commandList);
	void SetMSAARenderTarget(ID3D12GraphicsCommandList* commandList);
	void ResolveMSAAToBackBuffer(ID3D12GraphicsCommandList* commandList);
	void ClearBackBuffer(ID3D12GraphicsCommandList* commandList);
	void ClearDepthBuffer(ID3D12GraphicsCommandList* commandList);
	void SetDescriptorHeap(ID3D12DescriptorHeap* descriptorHeap);
	void SetGraphicsRootDescriptorTable(ID3D12DescriptorHeap* descriptorHeap, int cbvIndex, int rootParameterIndex);
	void SetDescriptorHeapsAndRootSignature(int thread, int list);
	void SwapBackBuffers(bool vSync);

	// Get samplers
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	// Execute commands on main command list
	void ExecuteCommands();

	// Close and execute base command list
	void CloseAndExecuteCommandList();

	// Close and execute command list for this thread
	void CloseAndExecuteCommandList(int thread, int list);

	// Cycle through frame resources
	void CycleFrameResources();

private:
	// Viewport and scissor rectangle
	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	ComPtr<IDXGIFactory4> mDXGIFactory;
	ComPtr<IDXGISwapChain> mSwapChain;

	static const unsigned int mSwapChainBufferCount = 2;

	ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];

	static const unsigned int mMaxThreads = 64;
	static const unsigned int mMaxCommandListsPerThread = 2;

	// Multithreading command objects
	ComPtr<ID3D12CommandAllocator> mCommandAllocators[mNumFrameResources][mMaxThreads];
	ComPtr<ID3D12GraphicsCommandList> mCommandLists[mMaxThreads][mMaxCommandListsPerThread];

	// Default background colour
	XMVECTORF32 mBackgroundColour = DirectX::Colors::Purple;

	// Render target for MSAA
	ComPtr<ID3D12Resource> mMSAARenderTarget;

	// MSAA variables
	UINT mMSAAQuality = 0;
	int mMSAASampleCount = 4;

	// Backbuffer width and height
	int mBackbufferWidth;
	int mBackbufferHeight;

	// Current back buffer
	int mCurrentBackBuffer = 0;

	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Setup functions
	bool CreateDeviceAndFence();
	void CreateRootSignature();
	void CreatePSO();
	void CreateShaders();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hWND, int width, int height);
	void CreateDescriptorHeaps();
	void CreateBlendState();

};



