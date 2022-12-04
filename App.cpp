#include "App.h"
#include <sdl_syswm.h>

// Construct and set reference to self for WndProc
App::App()
{
	Initialize();

	std::string windowText = mMainCaption;
	const char* array = windowText.c_str();
	SDL_SetWindowTitle(mWindow, array);

	Run();
}

bool App::InitWindow()
{
	return true;
}

// Run the app
void App::Run()
{

	MSG msg = {};

	// Start timer for frametime
	mTimer.Start();

	while (!mQuit)
	{
		SDL_Event event;
		//Handle events on queue
		while (SDL_PollEvent(&event) != 0)
		{
			//Handle window events
			PollEvents(event);
		}
		if (!mMinimized)
		{
			// Start the Dear ImGui frame
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			ShowGUI();

			float frameTime = mTimer.GetLapTime();
			Update(frameTime);

			Draw(frameTime);
		}
	}	
}

void App::Initialize()
{
	SDL_Init(SDL_INIT_VIDEO);
	mWindow = SDL_CreateWindow("D3D12 Masters Project",SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mWidth, mHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	
	SDL_SysWMinfo info;
	SDL_GetVersion(&info.version);
	SDL_GetWindowWMInfo(mWindow, &info);
	HWND hwnd = info.info.win.window;

	mGraphics = make_unique<Graphics>(hwnd, mWidth, mHeight, mNumStartingItems);
	
	UpdateCamera();
	CreateIcosohedron();
	CreateRenderItems();
	mGraphics->ExecuteCommands();

	SetupGUI();

	// Make initial projection matrix
	Resized();
}

void App::SetupGUI()
{
	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	auto cpuhandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mGraphics->mCBVHeap->GetCPUDescriptorHandleForHeapStart());
	cpuhandle.Offset(mGraphics->mGUISRVOffset, mGraphics->mCbvSrvUavDescriptorSize);

	auto gpuhandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mGraphics->mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	gpuhandle.Offset(mGraphics->mGUISRVOffset, mGraphics->mCbvSrvUavDescriptorSize);

	ImGui_ImplSDL2_InitForD3D(mWindow);
	ImGui_ImplDX12_Init(mGraphics->mD3DDevice.Get(), mGraphics->mNumFrameResources, mGraphics->mBackBufferFormat, mGraphics->mCBVHeap.Get(),
		cpuhandle, gpuhandle);
}

void App::ShowGUI()
{
	static bool showDemoWindow = false;
	//ImGui::ShowDemoWindow(&showDemoWindow);

	static float f = 0.0f;
	static int counter = 0;

	ImGui::Begin("Planet");

	ImGui::Text("Geometry");

	if (ImGui::SliderInt("Recursions", &recursions, 0, 12))
	{
		RecreateGeometry(mTesselation);
	};

	if (ImGui::Checkbox("Tesselation", &mTesselation))
	{
		RecreateGeometry(mTesselation);
	};

	ImGui::Text("Noise");

	static float prevFreq = 0;
	if (ImGui::SliderFloat("Noise Frequency", &frequency, 0.0f, 1.0f, "%.1f"))
	{
		RecreateGeometry(mTesselation);
	};

	if (ImGui::SliderInt("Octaves", &octaves, 0, 20))
	{
		RecreateGeometry(mTesselation);
	};

	ImGui::Text("World Matrix");

	static float pos[3];
	if (ImGui::SliderFloat3("Position", pos, -5.0f, 5.0f, "%.1f"))
	{
		XMStoreFloat4x4(&mIcoWorldMatrix, XMMatrixIdentity() * XMMatrixTranslation(pos[0], pos[1], pos[2]));

		// Signal to update object constant buffer
		mRenderItems[0]->NumDirtyFrames += mGraphics->mNumFrameResources;

		//RecreateGeometry();
	}

	static float rot[3];
	if (ImGui::SliderFloat3("Rotation", rot, -5.0f, 5.0f, "%.1f"))
	{
		
		mGraphics->mCurrentFrameResource->mPerObjectConstantBuffer;
		XMStoreFloat4x4(&mIcoWorldMatrix, XMMatrixIdentity() * XMMatrixRotationX(rot[0]) * XMMatrixRotationY(rot[1]) * XMMatrixRotationZ(rot[2]));

		// Signal to update object constant buffer
		mRenderItems[0]->NumDirtyFrames += mGraphics->mNumFrameResources;

		//RecreateGeometry();
	}

	static float scale = 1;
	if (ImGui::SliderFloat("Scale", &scale, 0.0f, 5.0f, "%.1f"))
	{
		XMStoreFloat4x4(&mIcoWorldMatrix, XMMatrixIdentity() * XMMatrixScaling(scale, scale, scale));

		// Signal to update object constant buffer
		mRenderItems[0]->NumDirtyFrames += mGraphics->mNumFrameResources;

		//RecreateGeometry();
	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

}

void App::CreateIcosohedron()
{
	if (!mIcosohedron) { mIcosohedron.release(); }
	mIcosohedron = std::make_unique<Icosahedron>(frequency, recursions, octaves, mEyePos, mTesselation);
}

void App::CreateRenderItems()
{
	if (mRenderItems.size() != 0)
	{
		for (auto& item : mRenderItems)
		{
			delete item;
		}
		mRenderItems.clear();
	}

	RenderItem* icoRenderItem = new RenderItem();
	icoRenderItem->WorldMatrix = mIcoWorldMatrix;
	icoRenderItem->ObjConstantBufferIndex = 0;
	icoRenderItem->Geometry = mIcosohedron->mGeometryData.get();
	icoRenderItem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	icoRenderItem->IndexCount = mIcosohedron->mGeometryData->mIndices.size();
	icoRenderItem->StartIndexLocation = 0;
	icoRenderItem->BaseVertexLocation = 0;
	mRenderItems.push_back(icoRenderItem);

	//RenderItem* icoRenderItem2 = new RenderItem();
	//XMStoreFloat4x4(&icoRenderItem2->WorldMatrix, XMMatrixIdentity() * XMMatrixTranslation(2.0f, 0.0f,0.0f));
	//icoRenderItem2->ObjConstantBufferIndex = 1;
	//icoRenderItem2->Geometry = mIcosohedron->mGeometryData.get();
	//icoRenderItem2->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//icoRenderItem2->IndexCount = mIcosohedron->mGeometryData->mIndices.size();
	//icoRenderItem2->StartIndexLocation = 0;
	//icoRenderItem2->BaseVertexLocation = 0;
	//mRenderItems.push_back(icoRenderItem2);

	//if (FAILED(mGraphics->mCommandList->Reset(mGraphics->mCommandAllocator.Get(), nullptr)))
	//{
	//	MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	//}
}

void App::RecreateGeometry(bool tesselation)
{
	static bool firstFrame = true;
	if (!firstFrame)
	{
		mIcosohedron->ResetGeometry(mEyePos, frequency, recursions, octaves, tesselation);
		mIcosohedron->CreateGeometry();
	}
	else { firstFrame = false; }

	for (int i = 0; i < mIcosohedron->mVertices.size(); i++)
	{
		mGraphics->mCurrentFrameResource->mPlanetVB->Copy(i, mIcosohedron->mVertices[i]);
	}
	for (int i = 0; i < mIcosohedron->mIndices.size(); i++)
	{
		mGraphics->mCurrentFrameResource->mPlanetIB->Copy(i, mIcosohedron->mIndices[i]);
	}

	mRenderItems[0]->Geometry->mGPUVertexBuffer = mGraphics->mCurrentFrameResource->mPlanetVB->GetBuffer();
	mRenderItems[0]->Geometry->mGPUIndexBuffer = mGraphics->mCurrentFrameResource->mPlanetIB->GetBuffer();
	mRenderItems[0]->WorldMatrix = mIcoWorldMatrix;
	mRenderItems[0]->IndexCount = mIcosohedron->mIndices.size();
}


void App::Update(float frameTime)
{
	UpdateCamera();

	mGraphics->CycleFrameResources();
	
	static bool firstGenerationFrame = true;
	if (firstGenerationFrame)
	{
		RecreateGeometry(mTesselation);
		firstGenerationFrame = false;
	}

	if (mTesselation)
	{
		RecreateGeometry(mTesselation);
	}

	UpdatePerObjectConstantBuffers();
	UpdatePerFrameConstantBuffers();
}

void App::UpdatePerObjectConstantBuffers()
{
	auto currentObjectConstantBuffer = mGraphics->mCurrentFrameResource->mPerObjectConstantBuffer.get();
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
	auto currentPassCB = mGraphics->mCurrentFrameResource->mPerFrameConstantBuffer.get();
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

void App::Draw(float frameTime)
{
	auto commandAllocator = mGraphics->mCurrentFrameResource->mCommandAllocator;

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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mGraphics->mCBVHeap.Get() };
	mGraphics->mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mGraphics->mCommandList->SetGraphicsRootSignature(mGraphics->mRootSignature.Get());

	int passCbvIndex = mGraphics->mPassCbvOffset + mGraphics->mCurrentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mGraphics->mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mGraphics->mCbvSrvUavDescriptorSize);
	mGraphics->mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(mGraphics->mCommandList.Get());

	mGraphics->ResolveMSAAToBackBuffer(mGraphics->mCommandList.Get());

	RenderGUI();

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
	mGraphics->mCurrentFrameResource->Fence = mGraphics->mCurrentFence++;

	// Tell command queue to set new fence point, will only be set when the GPU gets to new fence value.
	mGraphics->mCommandQueue->Signal(mGraphics->mFence.Get(), mGraphics->mCurrentFence);

	// Frame buffering broken in debug mode so wait each frame
	//mGraphics->EmptyCommandQueue();
}

void App::RenderGUI()
{
	ImGui::Render();

	// Transition back buffer to RT
	mGraphics->mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGraphics->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapView(mGraphics->mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	dsvHeapView.Offset(1, mGraphics->mDsvDescriptorSize);

	// Select Back buffer as render target
	mGraphics->mCommandList->ClearDepthStencilView(dsvHeapView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGraphics->mCommandList->OMSetRenderTargets(1, &mGraphics->CurrentBackBufferView(), true, &dsvHeapView);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mGraphics->mCommandList.Get());

	// Transition back buffer to present
	mGraphics->mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGraphics->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
}

void App::DrawRenderItems(ID3D12GraphicsCommandList* commandList)
{
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));
	auto objectCB = mGraphics->mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();

	for (size_t i = 0; i < mRenderItems.size(); ++i)
	{
		auto renderItem = mRenderItems[i];
		commandList->IASetVertexBuffers(0, 1, &renderItem->Geometry->GetVertexBufferView());
		commandList->IASetIndexBuffer(&renderItem->Geometry->GetIndexBufferView());
		commandList->IASetPrimitiveTopology(renderItem->Topology);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = mGraphics->mCurrentFrameResourceIndex * (UINT)mRenderItems.size()+ renderItem->ObjConstantBufferIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mGraphics->mCBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mGraphics->mCbvSrvUavDescriptorSize);
		commandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
		commandList->DrawIndexedInstanced(renderItem->IndexCount, 1, renderItem->StartIndexLocation, renderItem->BaseVertexLocation, 0);
	}
}


void App::MouseMoved(SDL_Event& event)
{
	int mouseX, mouseY;
	mouseX = event.motion.x;
	mouseY = event.motion.y;
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

		//// Restrict the radius.
		//mRadius = std::clamp(mRadius, 3.0f, 15.0f);
	}
	mLastMousePos.x = mouseX;
	mLastMousePos.y = mouseY;
}

void App::PollEvents(SDL_Event& event)
{
	//Window event occured
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplSDL2_ProcessEvent(&event);
	if (io.WantCaptureMouse || io.WantCaptureKeyboard)
	{
		return;
	}
	if (event.type == SDL_WINDOWEVENT)
	{		
		switch (event.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			mWidth = event.window.data1;
			mHeight = event.window.data2;
			mGraphics->Resize(mWidth, mHeight);
			Resized();
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			break;
		case SDL_WINDOWEVENT_ENTER:
			mMouseFocus = true;
			break;
		case SDL_WINDOWEVENT_LEAVE:
			mMouseFocus = false;
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			mKeyboardFocus = true;
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			mKeyboardFocus = false;
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			mMinimized = true;
			mTimer.Stop();
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			mMinimized = false;
			break;
		case SDL_WINDOWEVENT_RESTORED:
			mMinimized = false;
			mTimer.Start();
			break;
		}
	}
	else if (event.type == SDL_KEYDOWN)
	{
		auto key = event.key.keysym.sym;
		if (key == SDLK_F11)
		{	
			if (mFullscreen)
			{
				SDL_SetWindowFullscreen(mWindow, SDL_FALSE);
			}
			else
			{
				SDL_SetWindowFullscreen(mWindow, SDL_TRUE);
			}
			mFullscreen = !mFullscreen;
		}
	}
	else if (event.type == SDL_MOUSEMOTION)
	{
		MouseMoved(event);
	}
	else if (event.type == SDL_QUIT)
	{
		mQuit = true;
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN)
	{
		if (event.button.button == 3) { mRightMouse = true; }
		else if (event.button.button == 1) { mLeftMouse = true; }
		else if (event.button.button == 2) { mGraphics->mWireframe = !mGraphics->mWireframe; mGraphics->CreatePSO(); }
	}
	else if (event.type == SDL_MOUSEBUTTONUP)
	{
		if (event.button.button == 3) { mRightMouse = false; }
		else if (event.button.button == 1) { mLeftMouse = false; }
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
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }

	//delete mRenderItems[1]->Geometry;
	for (auto& renderItem : mRenderItems)
	{
		delete renderItem;
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyWindow(mWindow);
	SDL_Quit();
}