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
//#include "DDSTextureLoader.h"
#include "Timer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

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
	
	void OnMouseDown(WPARAM buttonState, int x,int y){ }
	void OnMouseUp(WPARAM buttonState, int x, int y) {}
	void OnMouseMove(WPARAM buttonState, int x, int y) {}

	bool InitWindow();
	bool InitDirect3D();

	void CreateCommandObjects(); // Creates command queue, command list allocator, and a command list
	void CreateSwapChain();
	void FlushCommandQueue(); // Force CPU to wait for GPU to finish processing commands in queue

	ID3D12Resource* CurrentBackBuffer(); // Returns current back buffer in swap chain
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView(); // Returns Render Target View to current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView(); // Returns Depth / Stencil View  to main depth buffer

	void CalcFrameStats(); // Calculates average FPS and ms per frame

	//void LogAdapters(); // Enumerates adapters on system
	//void LogAdapterOutputs(IDXGIAdapter* adapter); // Enumerates all outputs associated with given adapter
	//void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format); // Enumerates all display modes an output supports for format

private:
	static D3DApp* mApp;

	HINSTANCE mHInst = nullptr; // App instance handle
	HWND mHWND = nullptr; // Window handle

	bool mAppPaused = false;
	bool mMinimised = false;
	bool mMaximised = false;
	bool mResizing = false;
	bool mFullscreen = false;

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

	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	std::wstring mMainCaption = L"D3D App";
	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mWidth = 1920;
	int mHeight = 1080;
};

