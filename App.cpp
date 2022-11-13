#include "App.h"
#include "WindowsX.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd as messages can be received before hwnd is valid
	return App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

// Reference to pass to WndProc to take control of msg procedures
App* App::mApp = nullptr;
App* App::GetApp() { return mApp;}

// Construct and set reference to self for WndProc
App::App(HINSTANCE hInstance) : mHInstance(hInstance)
{
    // Only allow one app to be created
    assert(mApp == nullptr);
    mApp = this;
}

bool App::InitWindow()
{
	// Register windows class
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mHInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"Wnd";
	if (!RegisterClass(&wc)) { MessageBox(0, L"Register Class Failed.", L"Error", MB_OK); return false; }

	// Calc window dimensions
	RECT rect = { 0, 0, mWidth, mHeight };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	// Create window and handle
	mHWND = CreateWindow(L"Wnd", L"D3D12", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mHInstance, 0);
	if (!mHWND)
	{
		MessageBox(0, L"Create Window Failed.", L"Error", MB_OK);
		return false;
	}

	ShowWindow(mHWND, SW_SHOW);
	UpdateWindow(mHWND);

	return true;
}

// Calculate stats for rendering frame
void App::CalcFrameStats()
{
	static int frameCount = 0;
	static float timeElapsed = 0.0f;

	frameCount++;

	// Calculate averages
	if ((mTimer.GetTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCount;
		float ms = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring msStr = std::to_wstring(ms);

		std::wstring windowText = mMainCaption + L"    fps: " + fpsStr + L"   ms: " + msStr;

		SetWindowText(mHWND, windowText.c_str());

		// Reset
		frameCount = 0;
		timeElapsed += 1.0f;
	}
}

// Run the app
int App::Run()
{
	MSG msg = {};

	// Start timer for frametime
	mTimer.Start();

	// While window is open
	while (msg.message != WM_QUIT)
	{
		// Process windows messages
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else // No messages left
		{
			// Get frametime from timer
			float frameTime = mTimer.GetLapTime();

			// If app isnt paused
			if (!mAppPaused)
			{
				CalcFrameStats();
				Update(frameTime);
				Draw(frameTime);
			}
			else { Sleep(100); }
		}
	}

	return (int)msg.wParam;
}

bool App::Initialize()
{
	if (!InitWindow()) { return false; }
	mGraphics = make_unique<Graphics>(mHWND, mWidth, mHeight);

	//// Break on D3D12 errors
	//ID3D12InfoQueue* infoQueue = nullptr;
	//mGraphics->mD3DDevice->QueryInterface(IID_PPV_ARGS(&infoQueue));

	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);

	//infoQueue->Release();

	CreateDescriptorHeaps();

	mGraphics->OnResize(mWidth, mHeight); // Initial resize

	if (FAILED(mGraphics->mCommandList->Reset(mGraphics->mCommandAllocator.Get(), nullptr)))
	{
		MessageBox(0, L"Command Allocator reset failed", L"Error", MB_OK);
	}

	CreateConstantBuffers();
	CreateRootSignature();
	CreateShaders();
	CreateIcosohedron();
	CreatePSO();

	// Execute initialization commands
	mGraphics->mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mGraphics->mCommandList.Get() };
	mGraphics->mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait until initialization is complete
	mGraphics->FlushCommandQueue();

	return true;
}

void App::Update(float frameTime)
{
	// Convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mGraphics->mViewMatrix, view);

	XMMATRIX world = XMLoadFloat4x4(&mGraphics->mWorldMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&mGraphics->mProjectionMatrix);
	XMMATRIX worldViewProj = world * view * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	mPerObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mPerObjectConstantBuffer->Copy(0,objConstants);
}

void App::Draw(float frameTime)
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	if (FAILED(mGraphics->mCommandAllocator->Reset()))
	{
		MessageBox(0, L"Command Allocator reset failed", L"Error", MB_OK);
	}

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (FAILED(mGraphics->mCommandList->Reset(mGraphics->mCommandAllocator.Get(), mPSO.Get())))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mGraphics->mCommandList->RSSetViewports(1, &mGraphics->mViewport);
	mGraphics->mCommandList->RSSetScissorRects(1, &mGraphics->mScissorRect);

	// Indicate a state transition on the resource usage.
	mGraphics->mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGraphics->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mGraphics->mCommandList->ClearRenderTargetView(mGraphics->CurrentBackBufferView(), DirectX::Colors::Purple, 0, nullptr);
	mGraphics->mCommandList->ClearDepthStencilView(mGraphics->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mGraphics->mCommandList->OMSetRenderTargets(1, &mGraphics->CurrentBackBufferView(), true, &mGraphics->DepthStencilView());


	ID3D12DescriptorHeap* descriptorHeaps[] = { mCBVHeap.Get() };
	mGraphics->mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mGraphics->mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mGraphics->mCommandList->IASetVertexBuffers(0, 1, &mIcosohedron->mGeometryData->GetVertexBufferView());
	mGraphics->mCommandList->IASetIndexBuffer(&mIcosohedron->mGeometryData->GetIndexBufferView());
	mGraphics->mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mGraphics->mCommandList->SetGraphicsRootDescriptorTable(0, mCBVHeap->GetGPUDescriptorHandleForHeapStart());

	mGraphics->mCommandList->DrawIndexedInstanced(mIcosohedron->mGeometryData->indicesCount, 1, 0, 0, 0);

	// Indicate a state transition on the resource usage.
	mGraphics->mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGraphics->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	if (FAILED(mGraphics->mCommandList->Close()))
	{
		MessageBox(0, L"Command List close failed", L"Error", MB_OK);
	}

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mGraphics->mCommandList.Get() };
	mGraphics->mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	if (FAILED(mGraphics->mSwapChain->Present(0, 0)))
	{
		MessageBox(0, L"Swap chain present failed", L"Error", MB_OK);
	}
	mGraphics->mCurrentBackBuffer = (mGraphics->mCurrentBackBuffer + 1) % mGraphics->mSwapChainBufferCount;

	// Wait until frame commands are complete.
	mGraphics->FlushCommandQueue();
}

void App::OnMouseMove(WPARAM buttonState, int x, int y)
{
	if ((buttonState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = std::clamp(mPhi, 0.1f, PI - 0.1f);
	}
	else if ((buttonState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = std::clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void App::CreateConstantBuffers()
{
	mPerObjectConstantBuffer = make_unique<UploadBuffer<mPerObjectConstants>>(mGraphics->mD3DDevice.Get(), 1, true);

	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(mPerObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mPerObjectConstantBuffer->GetBuffer()->GetGPUVirtualAddress();
	// Offset to the ith object constant buffer in the buffer.
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	mGraphics->mD3DDevice->CreateConstantBufferView(&cbvDesc, mCBVHeap->GetCPUDescriptorHandleForHeapStart());
}

void App::CreateRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	if(FAILED(hr))
	{
		MessageBox(0, L"Serialize Root Signature failed", L"Error", MB_OK);
	}

	if (FAILED(mGraphics->mD3DDevice->CreateRootSignature( 0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature))))
	{
		MessageBox(0, L"Root Signature creation failed", L"Error", MB_OK);
	}
}

ComPtr<ID3DBlob> App::CompileShader(const std::wstring& filename,const D3D_SHADER_MACRO* defines,
											const std::string& entrypoint,const std::string& target)
{
	UINT compileFlags = 0;

	#if defined(DEBUG) || defined(_DEBUG)  
		compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#endif

	HRESULT hr;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if (errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());

	if (FAILED(hr))
	{
		MessageBox(0, L"Shader compile failed", L"Error", MB_OK);
	}

	return byteCode;
}

void App::CreateShaders()
{
	HRESULT hr = S_OK;

	mVSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "VS", "vs_5_0");
	mPSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "PS", "ps_5_0");
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}


void App::CreateIcosohedron()
{
	mIcosohedron = std::make_unique<Icosahedron>(8, 36, mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
}

void App::CreatePSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mVSByteCode->GetBufferPointer()),
		mVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mPSByteCode->GetBufferPointer()),
		mPSByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mGraphics->mBackBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = mGraphics->mDepthStencilFormat;
	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO))))
	{
		MessageBox(0, L"Pipeline State Creation failed", L"Error", MB_OK);
	}
}

void App::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = mGraphics->mSwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	if (FAILED(mGraphics->mD3DDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mGraphics->mRTVHeap.GetAddressOf()))))
	{
		MessageBox(0, L"Render Target View creation failed", L"Error", MB_OK);
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	if (FAILED(mGraphics->mD3DDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mGraphics->mDSVHeap.GetAddressOf()))))
	{
		MessageBox(0, L"Depth Stencil View creation failed", L"Error", MB_OK);
	}

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	if (FAILED(mGraphics->mD3DDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCBVHeap))))
	{
		MessageBox(0, L"Constant Buffer View creation failed", L"Error", MB_OK);
	}

}

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// Pause app when window becomes inactive
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WHen user changes window size
	case WM_SIZE:
		// Save new dimensions.
		mWidth = LOWORD(lParam);
		mHeight = HIWORD(lParam);
		if (mGraphics)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimised = true;
				mMaximised = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimised = false;
				mMaximised = true;
				mGraphics->OnResize(mWidth,mHeight);
			}
			else if (wParam == SIZE_RESTORED)
			{
				// Restoring from minimized
				if (mMinimised)
				{
					mAppPaused = false;
					mMinimised = false;
					mGraphics->OnResize(mWidth, mHeight);
				}

				// Restoring from maximized
				else if (mMaximised)
				{
					mAppPaused = false;
					mMaximised = false;
					mGraphics->OnResize(mWidth, mHeight);
				}
				else if (mResizing)
				{
					// Only resize when user has finished with drag bars
				}
				else
				{
					mGraphics->OnResize(mWidth, mHeight);
				}
			}
		}
		return 0;

		// Start resize
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// End resize and reset everything to new dimensions
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		mGraphics->OnResize(mWidth, mHeight);
		return 0;

		// When window is destroyed
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// When menu is active and an invalid key is pressed
	case WM_MENUCHAR:
		// Remove alt-enter beep
		return MAKELRESULT(0, MNC_CLOSE);

		// Prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

		// Mouse button down events
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Mouse button up events
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Mouse move event
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
		
		// Key up events
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

App::~App()
{
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->FlushCommandQueue(); }
}