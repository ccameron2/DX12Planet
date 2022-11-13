#include "Graphics.h"
#include "Utility.h"
#include <DirectXMath.h>


Graphics::Graphics(HWND hWND, int width, int height)
{
	InitD3D();
	CreateCommandObjects();
	CreateSwapChain(hWND, width, height);
}

Graphics::~Graphics()
{

}

bool Graphics::InitD3D()
{

#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();
	}
#endif

	// Create DGXIFactory
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&mDXGIFactory))))
	{
		MessageBox(0, L"Factory creation failed", L"Error", MB_OK);
	}

	// Create hardware device
	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mD3DDevice))))
	{
		// Fallback device
		ComPtr<IDXGIAdapter> pWarpAdapter;
		mDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));
		D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mD3DDevice));
	}

	// Create Fence
	if (FAILED(mD3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence))))
	{
		MessageBox(0, L"Fence creation failed", L"Error", MB_OK);
	}

	// Get descriptor sizes
	mRtvDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check MSAA quality support on back buffer
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	mD3DDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels));
	//m4xMSAAQuality = msQualityLevels.NumQualityLevels;
	//assert(m4xMSAAQuality > 0 && "Unexpected MSAA quality level.");


	return true;
}

void Graphics::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	// Create Command Queue
	if (FAILED(mD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue))))
	{
		MessageBox(0, L"Command Queue creation failed", L"Error", MB_OK);
	}

	// Create Command Allocator
	if (FAILED(mD3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.GetAddressOf()))))
	{
		MessageBox(0, L"Command Allocator creation failed", L"Error", MB_OK);
	}

	// Create Command List
	if (FAILED(mD3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf()))))
	{
		MessageBox(0, L"Command List creation failed", L"Error", MB_OK);
	}

	// Close the command list 
	mCommandList->Close();
}

void Graphics::CreateSwapChain(HWND hWND, int width, int height)
{
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	// Create swap chain description
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = mSwapChainBufferCount;
	sd.OutputWindow = hWND;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Create swap chain with description
	if (FAILED(mDXGIFactory->CreateSwapChain(mCommandQueue.Get(), &sd, mSwapChain.GetAddressOf())))
	{
		MessageBox(0, L"Swap chain creation failed", L"Error", MB_OK);
	}
}

void Graphics::OnResize(int width, int height)
{
	assert(mD3DDevice);
	assert(mSwapChain);
	assert(mCommandAllocator);

	FlushCommandQueue();
	if (FAILED(mCommandList->Reset(mCommandAllocator.Get(), nullptr)))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	// Release previous resources
	for (int i = 0; i < mSwapChainBufferCount; i++)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain
	if (FAILED(mSwapChain->ResizeBuffers(mSwapChainBufferCount, width, height, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)))
	{
		MessageBox(0, L"Swap chain resize failed", L"Error", MB_OK);
	}
	mCurrentBackBuffer = 0;

	// Create render target for each buffer in swapchain
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < mSwapChainBufferCount; i++)
	{
		if (FAILED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i]))))
		{
			MessageBox(0, L"Get Swap chain buffer failed", L"Error", MB_OK);
		}
		mD3DDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth / stencil buffer description
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Create depth buffer with description
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	if (FAILED(mD3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()))))
	{
		MessageBox(0, L"Depth Stencil Buffer creation failed", L"Error", MB_OK);
	}

	// Create Descriptor to mip lvl 0 of entire resource using format from depth stencil
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	mD3DDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute resize commands
	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait for risize to be complete
	FlushCommandQueue();

	// Update viewport transform to area
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = static_cast<float>(width);
	mViewport.Height = static_cast<float>(height);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, width, height };

	float PI = 3.14159;
	// The window resized, so update the aspect ratio and recompute projection matrix.
	XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * PI, static_cast<float>(width) / height, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProjectionMatrix, p);

}

// Empty the command queue
void Graphics::FlushCommandQueue()
{
	// Increase fence to mark commands up to this point
	mCurrentFence++;

	// Set new fence point
	if (FAILED(mCommandQueue->Signal(mFence.Get(), mCurrentFence)))
	{
		MessageBox(0, L"Command queue signal failed", L"Error", MB_OK);
	}

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		if (FAILED(mFence->SetEventOnCompletion(mCurrentFence, eventHandle)))
		{
			MessageBox(0, L"Fence set event failed", L"Error", MB_OK);
		}

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

// Return current back buffer
ID3D12Resource* Graphics::CurrentBackBuffer()
{
	return mSwapChainBuffer[mCurrentBackBuffer].Get();
}

// Return view for current back buffer
D3D12_CPU_DESCRIPTOR_HANDLE Graphics::CurrentBackBufferView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), mCurrentBackBuffer, mRtvDescriptorSize);
}

// Return depth stencil view
D3D12_CPU_DESCRIPTOR_HANDLE Graphics::DepthStencilView()
{
	return mDSVHeap->GetCPUDescriptorHandleForHeapStart();
}

XMFLOAT4X4 Graphics::MakeIdentity4x4()
{
	XMFLOAT4X4 I(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	return I;
}