#include "App.h"

// Construct and set reference to self for WndProc
App::App()
{
	Initialize();
	Run();
}

bool App::InitWindow()
{
	return true;
}

// Calculate stats for rendering frame
void App::FrameStats()
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

		mWindow.setTitle(windowText);
		// Reset
		frameCount = 0;
		timeElapsed += 1.0f;
	}
}

// Run the app
void App::Run()
{
	MSG msg = {};

	// Start timer for frametime
	mTimer.Start();

	while (mWindow.isOpen())
	{
		PollEvents();
		float frameTime = mTimer.GetLapTime();
		FrameStats();
		Update(frameTime);
		Draw(frameTime);
	}	
}

void App::Initialize()
{
	mWindow.create(sf::VideoMode(mWidth, mHeight), "D3D12 Masters Project", sf::Style::Default);

	mGraphics = make_unique<Graphics>(mWindow.getSystemHandle(), mWidth, mHeight);
	
	CreateIcosohedron();
	CreateRenderItems();
	BuildFrameResources();
	CreateCBVHeap();
	CreateConstantBuffers();
	mGraphics->ExecuteCommands();

	// Make initial projection matrix
	Resized();
}

void App::BuildFrameResources()
{
	for (int i = 0; i < mNumFrameResources; i++)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mGraphics->mD3DDevice.Get(), 1, mRenderItems.size()));
	}
}

void App::UpdatePerObjectConstantBuffers()
{
	auto currentObjectConstantBuffer = mCurrentFrameResource->mPerObjectConstantBuffer.get();
	for (auto& rItem : mRenderItems)
	{
		if (rItem->NumDirtyFrames > 0)
		{
			XMMATRIX worldMatrix = XMLoadFloat4x4(&rItem->WorldMatrix);
			FrameResource::mPerObjectConstants objectConstants;
			XMStoreFloat4x4(&objectConstants.WorldMatrix, XMMatrixTranspose(worldMatrix));
			currentObjectConstantBuffer->Copy(rItem->ObjConstantBufferIndex, objectConstants);
			rItem->NumDirtyFrames--;
		}
	}
}

void App::UpdatePerFrameConstantBuffers()
{
	FrameResource::mPerFrameConstants perFrameConstantBuffer;
	XMMATRIX view = XMLoadFloat4x4(&mViewMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&mProjectionMatrix);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMStoreFloat4x4(&perFrameConstantBuffer.ViewMatrix, XMMatrixTranspose(view));
	XMStoreFloat4x4(&perFrameConstantBuffer.ProjMatrix, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&perFrameConstantBuffer.ViewProjMatrix, XMMatrixTranspose(viewProj));
	perFrameConstantBuffer.EyePosW = mEyePos;
	perFrameConstantBuffer.RenderTargetSize = XMFLOAT2((float)mWidth, (float)mHeight);
	perFrameConstantBuffer.NearZ = 1.0f;
	perFrameConstantBuffer.FarZ = 1000.0f;
	perFrameConstantBuffer.TotalTime = mTimer.GetTime();
	perFrameConstantBuffer.DeltaTime = mTimer.GetLapTime();
	auto currentPassCB = mCurrentFrameResource->mPerFrameConstantBuffer.get();
	currentPassCB->Copy(0, perFrameConstantBuffer);
}

void App::UpdateCamera()
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mViewMatrix, view);
}

void App::Update(float frameTime)
{	
	UpdateCamera();

	// Cycle frame resources
	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % mNumFrameResources;
	mCurrentFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

	UINT64 completedFence = mGraphics->mFence->GetCompletedValue();

	// Wait for GPU to finish current frame resource commands
	if (mCurrentFrameResource->Fence != 0 && completedFence < mCurrentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		if (FAILED(mGraphics->mFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle)))
		{
			MessageBox(0, L"Fence completion event set failed", L"Error", MB_OK);
		}
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdatePerObjectConstantBuffers();
	UpdatePerFrameConstantBuffers();
}

void App::Draw(float frameTime)
{
	auto commandAllocator = mCurrentFrameResource->mCommandAllocator;

	// We can only reset when the associated command lists have finished execution on the GPU.
	if (FAILED(commandAllocator->Reset()))
	{
		MessageBox(0, L"Command Allocator reset failed", L"Error", MB_OK);
	}

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	if (FAILED(mGraphics->mCommandList->Reset(commandAllocator.Get(), mGraphics->mPSO.Get())))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mGraphics->mCommandList->RSSetViewports(1, &mGraphics->mViewport);
	mGraphics->mCommandList->RSSetScissorRects(1, &mGraphics->mScissorRect);

	// Clear the back buffer and depth buffer.
	mGraphics->mCommandList->ClearRenderTargetView(mGraphics->MSAAView(), mGraphics->mBackgroundColour, 0, nullptr);
	mGraphics->mCommandList->ClearDepthStencilView(mGraphics->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Select MSAA texture as render target
	mGraphics->mCommandList->OMSetRenderTargets(1, &mGraphics->MSAAView(), true, &mGraphics->DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCBVHeap.Get() };
	mGraphics->mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mGraphics->mCommandList->SetGraphicsRootSignature(mGraphics->mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mGraphics->mCbvSrvUavDescriptorSize);
	mGraphics->mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(mGraphics->mCommandList.Get());

	mGraphics->ResolveMSAAToBackBuffer(mGraphics->mCommandList.Get());

	// Done recording commands.
	if (FAILED(mGraphics->mCommandList->Close()))
	{
		MessageBox(0, L"Command List close failed", L"Error", MB_OK);
	}

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdLists[] = { mGraphics->mCommandList.Get() };
	mGraphics->mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Swap the back and front buffers
	if (FAILED(mGraphics->mSwapChain->Present(0, 0)))
	{
		MessageBox(0, L"Swap chain present failed", L"Error", MB_OK);
	}
	mGraphics->mCurrentBackBuffer = (mGraphics->mCurrentBackBuffer + 1) % mGraphics->mSwapChainBufferCount;

	// Advance fence value
	mCurrentFrameResource->Fence = mGraphics->mCurrentFence++;

	// Tell command queue to set new fence point, will only be set when the GPU gets to new fence value.
	mGraphics->mCommandQueue->Signal(mGraphics->mFence.Get(), mGraphics->mCurrentFence);

	// Frame buffering broken so wait each frame
	mGraphics->EmptyCommandQueue();
}

void App::CreateIcosohedron()
{
	mIcosohedron = std::make_unique<Icosahedron>(8, 36, mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
}

void App::CreateConstantBuffers()
{
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));
	UINT numObjects = (UINT)mRenderItems.size();

	for (int frameIndex = 0; frameIndex < mNumFrameResources; frameIndex++)
	{
		auto objectConstantBuffer = mFrameResources[frameIndex]->mPerObjectConstantBuffer->GetBuffer();
		for (UINT i = 0; i < numObjects; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectConstantBuffer->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * numObjects + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mGraphics->mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			mGraphics->mD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	UINT passCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerFrameConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < mNumFrameResources; frameIndex++)
	{
		auto passCB = mFrameResources[frameIndex]->mPerFrameConstantBuffer->GetBuffer();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mGraphics->mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		mGraphics->mD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void App::CreateRenderItems()
{
	RenderItem* icoRenderItem = new RenderItem();
	XMStoreFloat4x4(&icoRenderItem->WorldMatrix, XMMatrixIdentity())/*XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f,0.0f))*/;
	icoRenderItem->ObjConstantBufferIndex = 0;
	icoRenderItem->Geometry = mIcosohedron->mGeometryData.get();
	icoRenderItem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	icoRenderItem->IndexCount = mIcosohedron->mGeometryData->mIndices.size();
	icoRenderItem->StartIndexLocation = 0;
	icoRenderItem->BaseVertexLocation = 0;
	mRenderItems.push_back(icoRenderItem);

	//// Create test cube
	//GeometryData* geometry = new GeometryData();
	//Cube cube;
	//geometry->mVertices = cube.vertices;
	//geometry->mIndices = cube.indices;
	////mGraphics->mCommandList->Reset(mGraphics->mCommandAllocator.Get(), nullptr);
	//geometry->CalculateBufferData(mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	//mGraphics->ExecuteCommands();
	//////////////

	//auto cubeRenderItem = new RenderItem();
	//XMStoreFloat4x4(&cubeRenderItem->WorldMatrix, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	//cubeRenderItem->ObjConstantBufferIndex = 0;
	//cubeRenderItem->Geometry = geometry;
	//cubeRenderItem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//cubeRenderItem->IndexCount = mIcosohedron->mGeometryData->mIndices.size();
	//cubeRenderItem->StartIndexLocation = 0;
	//cubeRenderItem->BaseVertexLocation = 0;
	//mRenderItems.push_back(std::move(cubeRenderItem));
}

void App::CreateCBVHeap()
{
	UINT objCount = (UINT)mRenderItems.size();
	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * mNumFrameResources;
	// Save an offset to the start of the pass CBVs. These are the last 3 descriptors.
	mPassCbvOffset = objCount * mNumFrameResources;
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	if (FAILED(mGraphics->mD3DDevice->CreateDescriptorHeap(&cbvHeapDesc,IID_PPV_ARGS(&mCBVHeap))))
	{
		MessageBox(0, L"Create descriptor heap failed", L"Error", MB_OK);
	}
}

void App::DrawRenderItems(ID3D12GraphicsCommandList* commandList)
{
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));
	auto objectCB = mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();

	for (size_t i = 0; i < mRenderItems.size(); ++i)
	{
		auto renderItem = mRenderItems[i];
		commandList->IASetVertexBuffers(0, 1, &renderItem->Geometry->GetVertexBufferView());
		commandList->IASetIndexBuffer(&renderItem->Geometry->GetIndexBufferView());
		commandList->IASetPrimitiveTopology(renderItem->Topology);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = mCurrentFrameResourceIndex * (UINT)mRenderItems.size()+ renderItem->ObjConstantBufferIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mGraphics->mCbvSrvUavDescriptorSize);
		commandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
		commandList->DrawIndexedInstanced(renderItem->IndexCount, 1,renderItem->StartIndexLocation, renderItem->BaseVertexLocation, 0);
	}
}


void App::MouseMoved(sf::Event event)
{
	int mouseX, mouseY;
	mouseX = event.mouseMove.x;
	mouseY = event.mouseMove.y;
	if (mLeftMouse)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(mouseX - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(mouseY - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = std::clamp(mPhi, 0.1f, XM_PI - 0.1f);
	}
	else if (mRightMouse)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(mouseX - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(mouseY - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = std::clamp(mRadius, 3.0f, 15.0f);
	}
	mLastMousePos.x = mouseX;
	mLastMousePos.y = mouseY;
}

void App::PollEvents()
{
	sf::Event event;
	while (mWindow.pollEvent(event))
	{
		sf::Vector2u size;
		switch (event.type)
		{
		case sf::Event::Closed:
			mWindow.close();
			break;
		case sf::Event::LostFocus:
			mAppPaused = true;
			mTimer.Stop();
			break;
		case sf::Event::GainedFocus:
			mAppPaused = false;
			mTimer.Start();
			break;
		case sf::Event::Resized:
			size = mWindow.getSize();
			mWidth = size.x; mHeight = size.y;
			mGraphics->Resize(mWidth, mHeight);
			Resized();
			mTimer.Start();
			break;
		case sf::Event::MouseButtonPressed:
			if (event.mouseButton.button == sf::Mouse::Right) { mRightMouse = true; }
			else if (event.mouseButton.button == sf::Mouse::Left) { mLeftMouse = true; }
			break;
		case sf::Event::MouseButtonReleased:
			if (event.mouseButton.button == sf::Mouse::Right) { mRightMouse = false; }
			else if (event.mouseButton.button == sf::Mouse::Left) { mLeftMouse = false; }
			break;
		case sf::Event::MouseMoved:
			MouseMoved(event);
			break;
		}
	}
}

void App::Resized()
{
	// The window resized, so update the aspect ratio and recompute projection matrix.
	XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(mWidth) / mHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProjectionMatrix, p);
}

App::~App()
{
	//delete mRenderItems[1]->Geometry;
	for (auto& renderItem : mRenderItems)
	{
		delete renderItem;
	}
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }
}