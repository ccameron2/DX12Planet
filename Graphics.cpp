#include "Graphics.h"
#include "Utility.h"
#include <DirectXMath.h>
#include <stdexcept>

UINT CbvSrvUavDescriptorSize = 0;
int CurrentFrameResourceIndex = 0;
ComPtr<ID3D12CommandQueue> CommandQueue;
ComPtr<ID3D12Device> D3DDevice;

Graphics::Graphics(HWND hWND, int width, int height)
{
	// Break on D3D12 errors
	CreateDeviceAndFence();

#if defined(DEBUG) || defined(_DEBUG) 
	ID3D12InfoQueue* infoQueue = nullptr;
	D3DDevice->QueryInterface(IID_PPV_ARGS(&infoQueue));

	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);

	infoQueue->Release();
#endif
	
	CreateCommandObjects();
	CreateSwapChain(hWND, width, height);
	CreateDescriptorHeaps();
	
	// Create SRV heap
	SrvDescriptorHeap = make_unique<SRVDescriptorHeap>(D3DDevice.Get(), CbvSrvUavDescriptorSize);

	// Initially resize
	Resize(width, height);

	// Create shaders and root sig
	CreateRootSignature();
	CreateShaders();
	CreatePSO();

	// Reset command list
	if (FAILED(mCommandList->Reset(mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr)))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}
}

Graphics::~Graphics()
{

}

void Graphics::ResolveMSAAToBackBuffer(ID3D12GraphicsCommandList* commandList)
{
	// Transition MSAA texture to resolve source
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mMSAARenderTarget.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE));

	// Transition Back buffer to resolve destination
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST));

	// Resolve MSAA to back buffer
	commandList->ResolveSubresource(CurrentBackBuffer(), 0, mMSAARenderTarget.Get(), 0, mBackBufferFormat);

	// Transition back buffer to present
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT));

	// Transition MSAA texture to render target for use next frame
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mMSAARenderTarget.Get(),
		D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
}

void Graphics::ResetCommandAllocator(ID3D12CommandAllocator* commandAllocator)
{
	// We can only reset when the associated command lists have finished execution on the GPU.
	if (FAILED(commandAllocator->Reset()))
	{
		MessageBox(0, L"Command Allocator reset failed", L"Error", MB_OK);
	}
}

void Graphics::ResetCommandList(ID3D12CommandAllocator* commandAllocator, ID3D12PipelineState* pipelineState)
{
	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	if (FAILED(mCommandList->Reset(commandAllocator, pipelineState)))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}
}

void Graphics::SetViewportAndScissorRects(ID3D12GraphicsCommandList* commandList)
{
	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	commandList->RSSetViewports(1, &mViewport);
	commandList->RSSetScissorRects(1, &mScissorRect);
}

void Graphics::ClearBackBuffer(ID3D12GraphicsCommandList* commandList)
{
	commandList->ClearRenderTargetView(MSAAView(), mBackgroundColour, 0, nullptr);
}

void Graphics::ClearDepthBuffer(ID3D12GraphicsCommandList* commandList)
{
	commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void Graphics::SetMSAARenderTarget(ID3D12GraphicsCommandList* commandList)
{
	commandList->OMSetRenderTargets(1, &MSAAView(), true, &DepthStencilView());
}

void Graphics::SetDescriptorHeap(ID3D12DescriptorHeap* descriptorHeap)
{
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void Graphics::SetDescriptorHeapsAndRootSignature(int thread, int list)
{
	ID3D12DescriptorHeap* heaps[] = { SrvDescriptorHeap->mHeap.Get() };
	mCommandLists[thread][list]->SetDescriptorHeaps(1, heaps);

	mCommandLists[thread][list]->SetGraphicsRootSignature(mRootSignature.Get());
}

void Graphics::SwapBackBuffers(bool vSync)
{
	// Swap the back and front buffers
	if (FAILED(mSwapChain->Present(vSync, 0)))
	{
		MessageBox(0, L"Swap chain present failed", L"Error", MB_OK);
	}
	mCurrentBackBuffer = (mCurrentBackBuffer + 1) % mSwapChainBufferCount;
}

void Graphics::CreateBlendState()
{
	mTransparencyBlendDesc.BlendEnable = true;
	mTransparencyBlendDesc.LogicOpEnable = false;
	mTransparencyBlendDesc.SrcBlend = D3D12_BLEND_ZERO;
	mTransparencyBlendDesc.DestBlend = D3D12_BLEND_SRC_COLOR;
	mTransparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	mTransparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	mTransparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	mTransparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	mTransparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	mTransparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

void Graphics::SetGraphicsRootDescriptorTable(ID3D12DescriptorHeap* descriptorHeap, int cbvIndex, int rootParameterIndex)
{
	auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cbvHandle.Offset(cbvIndex, CbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, cbvHandle);
}

void Graphics::CloseAndExecuteCommandList()
{
	// Close the command list
	if (FAILED(mCommandList->Close()))
	{
		MessageBox(0, L"Command List close failed", L"Error", MB_OK);
	}

	// Add the command list to the queue for execution
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

// Close a given thread's command list and commit its work to the GPU
void Graphics::CloseAndExecuteCommandList(int thread, int list)
{
	HRESULT hr = mCommandLists[thread][list]->Close();
	if (FAILED(hr))  throw std::runtime_error("Error closing command list");

	ID3D12CommandList* commandLists[] = { mCommandLists[thread][list].Get() };
	CommandQueue->ExecuteCommandLists(1, commandLists);
}

bool Graphics::CreateDeviceAndFence()
{

#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug3> debugController;
		D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		//debugController->SetEnableAutoName(true);
		//debugController->SetEnableGPUBasedValidation(true);
		debugController->EnableDebugLayer();
	}
#endif

	// Create DGXIFactory
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&mDXGIFactory))))
	{
		MessageBox(0, L"Factory creation failed", L"Error", MB_OK);
	}

	// Create hardware device
	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&D3DDevice))))
	{
		// Fallback device
		ComPtr<IDXGIAdapter> pWarpAdapter;
		mDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));
		D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&D3DDevice));
	}

	// Create Fence
	if (FAILED(D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence))))
	{
		MessageBox(0, L"Fence creation failed", L"Error", MB_OK);
	}

	// Get descriptor sizes
	mRtvDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CbvSrvUavDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check MSAA quality support
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = mMSAASampleCount;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	D3DDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels));
	mMSAAQuality = msQualityLevels.NumQualityLevels;

	return true;
}

void Graphics::CreateRootSignature()
{
	// Mesh Textures
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,7,0,0); // register t0

	// Cube map
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,0,1); // register t0 space 1

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0); // Frame
	slotRootParameter[2].InitAsConstantBufferView(1); // Obj
	slotRootParameter[3].InitAsConstantBufferView(2); // Mat
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSignature = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSignature.GetAddressOf(), errorBlob.GetAddressOf())))
	{
		MessageBox(0, L"Serialize Root Signature failed", L"Error", MB_OK);
	}

	if (errorBlob != nullptr)
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	if (FAILED(D3DDevice->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf()))))
	{
		MessageBox(0, L"Root Signature creation failed", L"Error", MB_OK);
	}
}

void Graphics::CreatePSO()
{
	// Create transparency state
	CreateBlendState();

	// Create PSO description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mColourInputLayout.data(), (UINT)mColourInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		// Set vertex shader
		reinterpret_cast<BYTE*>(mColourVSByteCode->GetBufferPointer()),
		mColourVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		// Set pixel shader
		reinterpret_cast<BYTE*>(mColourPSByteCode->GetBufferPointer()),
		mColourPSByteCode->GetBufferSize()
	};

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.MultisampleEnable = true;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = mMSAASampleCount;
	psoDesc.SampleDesc.Quality = mMSAAQuality - 1;
	psoDesc.DSVFormat = mDepthStencilFormat;
	
	// Create BaseColour PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mSolidPSO))))
	{
		MessageBox(0, L"Solid Pipeline State Creation failed", L"Error", MB_OK);
	}

	// Set fillmode to wireframe
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

	// Create Wireframe PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWireframePSO))))
	{
		MessageBox(0, L"Wireframe Pipeline State Creation failed", L"Error", MB_OK);
	}

	// Set planet shaders
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mPlanetVSByteCode->GetBufferPointer()),
		mPlanetVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mPlanetPSByteCode->GetBufferPointer()),
		mPlanetPSByteCode->GetBufferSize()
	};

	// Set fillmode back to solid
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	// Create Planet PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPlanetPSO))))
	{
		MessageBox(0, L"Planet Pipeline State Creation failed", L"Error", MB_OK);
	}

	// Set PBR shaders
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mTexVSByteCode->GetBufferPointer()),
		mTexVSByteCode->GetBufferSize()
	};
	psoDesc.InputLayout = { mTexInputLayout.data(), (UINT)mTexInputLayout.size() };
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mTexPSByteCode->GetBufferPointer()),
		mTexPSByteCode->GetBufferSize()
	};
	
	// Create PBR PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mTexPSO))))
	{
		MessageBox(0, L"PBR Pipeline State Creation failed", L"Error", MB_OK);
	}

	// Set albedo shaders
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mSimpleTexVSByteCode->GetBufferPointer()),
		mSimpleTexVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mSimpleTexPSByteCode->GetBufferPointer()),
		mSimpleTexPSByteCode->GetBufferSize()
	};

	// Create albedo PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mSimpleTexPSO))))
	{
		MessageBox(0, L"Simple Pipeline State Creation failed", L"Error", MB_OK);
	}

	// Disable culling
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// Set sky shaders
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mSkyVSByteCode->GetBufferPointer()),
		mSkyVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mSkyPSByteCode->GetBufferPointer()),
		mSkyPSByteCode->GetBufferSize()
	};

	// Create PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mSkyPSO))))
	{
		MessageBox(0, L"Sky Pipeline State Creation failed", L"Error", MB_OK);
	}

	// Set cull mode to back
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	
	// Change input layout
	psoDesc.InputLayout = { mColourInputLayout.data(), (UINT)mColourInputLayout.size() };

	// Set transparency blending
	psoDesc.BlendState.RenderTarget[0] = mTransparencyBlendDesc;

	// Set water shaders
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mColourVSByteCode->GetBufferPointer()),
		mColourVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mColourPSByteCode->GetBufferPointer()),
		mColourPSByteCode->GetBufferSize()
	};
	
	// Create water PSO
	if (FAILED(D3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWaterPSO))))
	{
		MessageBox(0, L"Water Pipeline State Creation failed", L"Error", MB_OK);
	}

}

void Graphics::CreateShaders()
{
	// Define base colour input layout
	mColourInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Define PBR input layout
	mTexInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Compile colour shaders
	mColourVSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "VS", "vs_5_1");
	mColourPSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "PS", "ps_5_1");

	// Compile PBR shaders
	mTexVSByteCode = CompileShader(L"Shaders\\texshader.hlsl", nullptr, "VS", "vs_5_1");
	mTexPSByteCode = CompileShader(L"Shaders\\texshader.hlsl", nullptr, "PS", "ps_5_1");

	// Compile albedo shaders
	mSimpleTexVSByteCode = CompileShader(L"Shaders\\simpletexshader.hlsl", nullptr, "VS", "vs_5_1");
	mSimpleTexPSByteCode = CompileShader(L"Shaders\\simpletexshader.hlsl", nullptr, "PS", "ps_5_1");

	// Compile planet shaders
	mPlanetVSByteCode = CompileShader(L"Shaders\\planetshader.hlsl", nullptr, "VS", "vs_5_1");
	mPlanetPSByteCode = CompileShader(L"Shaders\\planetshader.hlsl", nullptr, "PS", "ps_5_1");

	// Compile sky shaders
	mSkyVSByteCode = CompileShader(L"Shaders\\skyshader.hlsl", nullptr, "VS", "vs_5_1");
	mSkyPSByteCode = CompileShader(L"Shaders\\skyshader.hlsl", nullptr, "PS", "ps_5_1");
}

// Compile shader from file
ComPtr<ID3DBlob> Graphics::CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
	UINT compileFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif`

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;

	// Attempt to compile shader
	if (FAILED(D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors)))
	{
		MessageBox(0, L"Shader compile failed", L"Error", MB_OK);
	}

	// Output if errors reported
	if (errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());

	return byteCode;
}

void Graphics::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	// Create command queue
	if (FAILED(D3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CommandQueue))))
	{
		MessageBox(0, L"Command Queue creation failed", L"Error", MB_OK);
	}

	for (int i = 0; i < mNumFrameResources; i++)
	{
		// Create base command allocators
		if (FAILED(D3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mBaseCommandAllocators[i].GetAddressOf()))))
		{
			MessageBox(0, L"Command Allocator creation failed", L"Error", MB_OK);
		}
	}
	
	// Create base command list
	if (FAILED(D3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf()))))
	{
		MessageBox(0, L"Command List creation failed", L"Error", MB_OK);
	}

	// Create command allocators per-thread / per-frame
	for (int frame = 0; frame < mNumFrameResources; ++frame)
	{
		for (int thread = 0; thread < mMaxThreads; ++thread)
		{
			if (FAILED(D3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[frame][thread]))))
				MessageBox(0, L"Command Allocator creation failed", L"Error", MB_OK);
		}
	}

	// Create multiple command lists per-thread
	for (int thread = 0; thread < mMaxThreads; ++thread)
	{
		for (int list = 0; list < mMaxCommandListsPerThread; ++list)
		{
			if (FAILED(D3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[CurrentFrameResourceIndex][thread].Get(), NULL, IID_PPV_ARGS(&mCommandLists[thread][list]))))
			{
				MessageBox(0, L"Command Allocator creation failed", L"Error", MB_OK);
				throw std::runtime_error("Error creating command list");
			}
			mCommandLists[thread][list]->Close();
		}
	}

	// Close the command list 
	mCommandList->Close();
}

void Graphics::CreateSwapChain(HWND hWND, int width, int height)
{
	// Release the previous swapchain
	mSwapChain.Reset();

	// Create swap chain description
	DXGI_SWAP_CHAIN_DESC swapDesc;
	swapDesc.BufferDesc.Width = width;
	swapDesc.BufferDesc.Height = height;
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapDesc.BufferDesc.Format = mBackBufferFormat;
	swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.BufferCount = mSwapChainBufferCount;
	swapDesc.OutputWindow = hWND;
	swapDesc.Windowed = true;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Create swap chain with description
	if (FAILED(mDXGIFactory->CreateSwapChain(CommandQueue.Get(), &swapDesc, mSwapChain.GetAddressOf())))
	{
		MessageBox(0, L"Swap chain creation failed", L"Error", MB_OK);
	}
}

void Graphics::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = mSwapChainBufferCount + 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	if (FAILED(D3DDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRTVHeap.GetAddressOf()))))
	{
		MessageBox(0, L"Render Target View creation failed", L"Error", MB_OK);
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	if (FAILED(D3DDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDSVHeap.GetAddressOf()))))
	{
		MessageBox(0, L"Depth Stencil View creation failed", L"Error", MB_OK);
	}
}

void Graphics::Resize(int width, int height)
{
	assert(D3DDevice);
	assert(mSwapChain);
	assert(mBaseCommandAllocators[CurrentFrameResourceIndex]);
	
	// Empty the command queue and reset the command list
	EmptyCommandQueue();
	if (FAILED(mCommandList->Reset(mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr)))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	// Release previous resources
	for (int i = 0; i < mSwapChainBufferCount; i++){ mSwapChainBuffer[i].Reset();}
	mDepthStencilBuffer.Reset();
	mDepthStencilBufferGUI.Reset();

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
		D3DDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}
	
	// Create MSAA Render target
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(mBackBufferFormat,
		static_cast<UINT64>(width),
		static_cast<UINT>(height),
		1, 1, mMSAASampleCount, mMSAAQuality - 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	// Set background colour
	D3D12_CLEAR_VALUE clearValue = { mBackBufferFormat, {} };
	memcpy(clearValue.Color, mBackgroundColour, sizeof(clearValue.Color));

	D3DDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue, IID_PPV_ARGS(mMSAARenderTarget.ReleaseAndGetAddressOf()));


	// Create MSAA desc
	D3D12_RENDER_TARGET_VIEW_DESC msaaDesc;
	msaaDesc.Format = mBackBufferFormat;
	msaaDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
	D3DDevice->CreateRenderTargetView(mMSAARenderTarget.Get(), &msaaDesc, rtvHeapHandle);
	rtvHeapHandle.Offset(1, mRtvDescriptorSize);

	// Create the depth / stencil buffer description
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = mMSAASampleCount;
	depthStencilDesc.SampleDesc.Quality = mMSAAQuality - 1;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Create depth buffer with description
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	if (FAILED(D3DDevice->CreateCommittedResource(
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
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	D3DDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Create the depth / stencil buffer description
	D3D12_RESOURCE_DESC depthStencilDescGUI;
	depthStencilDescGUI.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDescGUI.Alignment = 0;
	depthStencilDescGUI.Width = width;
	depthStencilDescGUI.Height = height;
	depthStencilDescGUI.DepthOrArraySize = 1;
	depthStencilDescGUI.MipLevels = 1;
	depthStencilDescGUI.Format = mDepthStencilFormat;
	depthStencilDescGUI.SampleDesc.Count = 1;
	depthStencilDescGUI.SampleDesc.Quality = 0;
	depthStencilDescGUI.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDescGUI.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Create depth buffer with description
	if (FAILED(D3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDescGUI,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBufferGUI.GetAddressOf()))))
	{
		MessageBox(0, L"Depth Stencil Buffer creation failed", L"Error", MB_OK);
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapView(mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	dsvHeapView.Offset(1, mDsvDescriptorSize);

	// Create Descriptor to mip lvl 0 of entire resource using format from depth stencil
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDescGUI;
	dsvDescGUI.Flags = D3D12_DSV_FLAG_NONE;
	dsvDescGUI.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDescGUI.Format = mDepthStencilFormat;
	dsvDescGUI.Texture2D.MipSlice = 0;
	D3DDevice->CreateDepthStencilView(mDepthStencilBufferGUI.Get(), &dsvDesc, dsvHeapView);
	
	// Transition to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBufferGUI.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	ExecuteCommands();

	// Update viewport transform to area
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = float(width);
	mViewport.Height = float(height);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mScissorRect = { 0, 0, width, height };

	mBackbufferWidth = mScissorRect.right - mScissorRect.left;
	mBackbufferHeight = mScissorRect.bottom - mScissorRect.top;

}

void Graphics::ExecuteCommands()
{
	// Execute commands
	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait for work to be complete
	EmptyCommandQueue();
}

void Graphics::CycleFrameResources()
{
	// Cycle frame resources
	CurrentFrameResourceIndex = (CurrentFrameResourceIndex + 1) % mNumFrameResources;
	mCurrentFrameResource = FrameResources[CurrentFrameResourceIndex].get();

	UINT64 completedFence = mFence->GetCompletedValue();

	// Wait for GPU to finish current frame resource commands
	if (mCurrentFrameResource->Fence != 0 && completedFence < mCurrentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		if (FAILED(mFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle)))
		{
			MessageBox(0, L"Fence completion event set failed", L"Error", MB_OK);
		}
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void Graphics::EmptyCommandQueue()
{
	// Increase fence to mark commands up to this point
	mCurrentFence++;

	// Set new fence point
	if (FAILED(CommandQueue->Signal(mFence.Get(), mCurrentFence)))
	{
		MessageBox(0, L"Command queue signal failed", L"Error", MB_OK);
	}

	// Wait for GPU to complete commands up to fence point
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		
		// When GPU hits fence fire event
		if (FAILED(mFence->SetEventOnCompletion(mCurrentFence, eventHandle)))
		{
			MessageBox(0, L"Fence set event failed", L"Error", MB_OK);
		}

		// Wait for event to fire
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

}

void Graphics::ResetCommandAllocator(int thread)
{
	HRESULT hr = mCommandAllocators[CurrentFrameResourceIndex][thread]->Reset();
	if (FAILED(hr))  throw std::runtime_error("Error reseting command allocator");
}

ID3D12GraphicsCommandList* Graphics::StartCommandList(int thread, int list)
{
	HRESULT hr = mCommandLists[thread][list]->Reset(mCommandAllocators[CurrentFrameResourceIndex][thread].Get(), NULL);
	if (FAILED(hr))  throw std::runtime_error("Error reseting command list");
	return mCommandLists[thread][list].Get();
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

D3D12_CPU_DESCRIPTOR_HANDLE Graphics::MSAAView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), 2, mRtvDescriptorSize);
}
// Return depth stencil view
D3D12_CPU_DESCRIPTOR_HANDLE Graphics::DepthStencilView()
{
	return mDSVHeap->GetCPUDescriptorHandleForHeapStart();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Graphics::GetStaticSamplers()
{

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}