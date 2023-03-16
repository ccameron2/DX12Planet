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
	
	ComPtr<ID3D12Device> mD3DDevice;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;

	ComPtr<IDXGIFactory4> mDXGIFactory;
	ComPtr<IDXGISwapChain> mSwapChain;

	ComPtr<ID3D12Fence1> mFence;
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

	int mCurrentBackBuffer = 0;
	ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	ComPtr<ID3D12Resource> mDepthStencilBufferGUI;

	D3D12_CPU_DESCRIPTOR_HANDLE MSAAView();
	ComPtr<ID3D12Resource> mMSAARenderTarget;

	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;
	ComPtr<ID3D12RootSignature> mRootSignature;
	FrameResource* mCurrentFrameResource = nullptr;
	const static int mNumFrameResources = 3;

	XMVECTORF32 mBackgroundColour = DirectX::Colors::Purple;

	bool CreateDeviceAndFence();
	void CreateRootSignature();
	void CreatePSO();
	void CreateShaders();
	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	void CreateCommandObjects();
	void CreateSwapChain(HWND hWND, int width, int height);

	void Resize(int width, int height);
	void EmptyCommandQueue();

	ID3D12Resource* CurrentBackBuffer(); // Returns current back buffer in swap chain
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(); // Returns Render Target View to current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView(); // Returns Depth / Stencil View  to main depth buffer

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	ComPtr<ID3D12PipelineState> mSolidPSO = nullptr;
	ComPtr<ID3D12PipelineState> mWireframePSO = nullptr;
	ComPtr<ID3D12PipelineState> mTexPSO = nullptr;

	ComPtr<ID3D12PipelineState> mPlanetPSO = nullptr;


	ComPtr<ID3DBlob> mColourVSByteCode = nullptr;
	ComPtr<ID3DBlob> mColourPSByteCode = nullptr;
	ComPtr<ID3DBlob> mTexVSByteCode = nullptr;
	ComPtr<ID3DBlob> mTexPSByteCode = nullptr;
	ComPtr<ID3DBlob> mPlanetVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPlanetPSByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mColourInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTexInputLayout;

	void ExecuteCommands();
	void CycleFrameResources();
	void CreateDescriptorHeaps();
	void ResolveMSAAToBackBuffer();
	void ResetCommandAllocator(ID3D12CommandAllocator* commandAllocator);
	void ResetCommandList(ID3D12CommandAllocator* commandAllocator, ID3D12PipelineState* pipeline);
	void SetViewportAndScissorRects();
	void ClearBackBuffer();
	void ClearDepthBuffer();
	void SetMSAARenderTarget();
	void SetDescriptorHeap(ID3D12DescriptorHeap* descriptorHeap);
	void SetGraphicsRootDescriptorTable(ID3D12DescriptorHeap* descriptorHeap, int cbvIndex, int rootParameterIndex);
	void CloseAndExecuteCommandList();
	void SwapBackBuffers(bool vSync);
};



