#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

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
//#include "DDSTextureLoader.h"
#include "Timer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class D3DApp
{
public:
	D3DApp(HINSTANCE hInstance);

	// Flush command queue to prevent GPU crash on exit
	~D3DApp();

	static D3DApp* GetApp();

	int Run();

	bool Initialize();

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); // Windows procedure function for the main app window

private:
	void CreateDescriptorHeaps();
	void OnResize();
	void Update(float frameTime);
	void Draw(float frameTime);
	
	void OnMouseDown(WPARAM buttonState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mHWND); }
	void OnMouseUp(WPARAM buttonState, int x, int y) { ReleaseCapture(); }
	void OnMouseMove(WPARAM buttonState, int x, int y);

	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	void CreateConstantBuffers();
	void CreateRootSignature();
	void CreateShaders();
	void CreateCube();
	void CreatePSO();

	bool InitWindow();
	bool InitDirect3D();

	void CreateCommandObjects(); // Creates command queue, command list allocator, and a command list
	void CreateSwapChain();
	void FlushCommandQueue(); // Force CPU to wait for GPU to finish processing commands in queue

	ID3D12Resource* CurrentBackBuffer(); // Returns current back buffer in swap chain
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(); // Returns Render Target View to current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView(); // Returns Depth / Stencil View  to main depth buffer

	void CalcFrameStats(); // Calculates average FPS and ms per frame

	ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView();
	D3D12_INDEX_BUFFER_VIEW IndexBufferView();
	void DisposeUploaders();

	XMFLOAT4X4 Identity4x4();


private:
	static D3DApp* mApp;

	HINSTANCE mHInst = nullptr; // App instance handle
	HWND mHWND = nullptr; // Window handle

	bool mAppPaused = false;
	bool mMinimised = false;
	bool mMaximised = false;
	bool mResizing = false;
	bool mFullscreen = false;

	POINT mLastMousePos;
	float PI = 3.14159;

	//bool m4xMSAA = false;
	//bool m4xMSAAQuality = 0; // Quality level of MSAA

	Timer mTimer;

	ComPtr<IDXGIFactory4> mDXGIFactory;
	ComPtr<IDXGISwapChain> mSwapChain;
	ComPtr<ID3D12Device> mD3DDevice;

	ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12CommandAllocator> mCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrentBackBuffer = 0;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;

	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;
	ComPtr<ID3D12DescriptorHeap> mCBVHeap;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	// Per Object constant buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> mPerObjectConstantBuffer;
	struct mPerObjectConstants
	{
		XMFLOAT4X4 WorldViewProj;
	};
	BYTE* mPerObjMappedData = nullptr;
	UINT mPerObjElementByteSize = 0;
	int mPerObjElementCount = 1;
	/////////////////////////////

	ComPtr<ID3D12RootSignature> mRootSignature;

	// Shader variables
	ComPtr<ID3DBlob> mVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPSByteCode = nullptr;

	// Input layout
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	static UINT CalcConstantBufferByteSize(UINT byteSize);

	// Vertex structure
	struct Vertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	int mIndicesCount = 0;

	// Vertex and index buffers on CPU side
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	// Vertex and index buffers on GPU side
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	// Vertex and index buffer uploaders
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// Data about the buffers.
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	XMFLOAT4X4 mWorld = Identity4x4();
	XMFLOAT4X4 mView = Identity4x4();
	XMFLOAT4X4 mProj = Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	std::wstring mMainCaption = L"D3D App";
	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mWidth = 800;
	int mHeight = 600;
};

