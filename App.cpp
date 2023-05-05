#include "App.h"
#include <DDSTextureLoader.h>
#include <ResourceUploadBatch.h>

std::vector<std::unique_ptr<FrameResource>> FrameResources;
unique_ptr<SRVDescriptorHeap> SrvDescriptorHeap;
int CurrentSRVOffset = 1;

App::App()
{
	Initialize();
	Run();
}

// Run the app
void App::Run()
{
	// Start timer for frametime
	mTimer.Start();

	// Set initial frame resources
	mGraphics->CycleFrameResources();

	RecreatePlanetGeometry();

	while (!mWindow->mQuit)
	{
		StartFrame();
		if (!mWindow->mMinimized)
		{		
			float frameTime = mTimer.GetLapTime();
			Update(frameTime);
			Draw(frameTime);
			EndFrame();
		}
	}	
}

void App::Initialize()
{
	// Create window object
	mWindow = make_unique<Window>(800,600);

	// Get reference to window handle
	HWND hwnd = mWindow->GetHWND();

	// Create graphics object 
	mGraphics = make_unique<Graphics>(hwnd, mWindow->mWidth, mWindow->mHeight);
	
	// Create camera object
	mCamera = make_unique<Camera>(mWindow.get());

	// Update camera
	mCamera->Update();

	// Create planet object
	mPlanet = std::make_unique<Planet>(mGraphics.get());
	mPlanet->CreatePlanet(0.1, 1, 1, 1,rand());

	// Create model for planet and pass in planet mesh
	mPlanetModel = new Model("", mGraphics->mCommandList.Get(), mPlanet->mMesh);
	mPlanetModel->SetPosition(XMFLOAT3{ 0, 0, 0 },false);
	mPlanetModel->SetRotation(XMFLOAT3{ 0, 0, 0 }, false);
	mPlanetModel->SetScale(XMFLOAT3{ float(mPlanet->mScale), float(mPlanet->mScale), float(mPlanet->mScale) }, true);
	mPlanetModel->mParallax = false;
	mModels.push_back(mPlanetModel);

	LoadModels();

	CreateSkybox();

	mNumModels = mModels.size();

	// Index materials and add to list
	CreateMaterials();

	// Create frame resource objects
	BuildFrameResources();

	// Execute all commands recorded so far
	mGraphics->ExecuteCommands();

	// Create GUI object
	mGUI = make_unique<GUI>(SrvDescriptorHeap.get(), mWindow->mSDLWindow, D3DDevice.Get(),
		mGraphics->mNumFrameResources, mGraphics->mBackBufferFormat);

	// Start worker threads
	mNumRenderWorkers = std::thread::hardware_concurrency();
	if (mNumRenderWorkers == 0)  mNumRenderWorkers = 8;
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		mRenderWorkers[i].first.thread = std::thread(&App::RenderThread, this, i);
	}

}

void App::StartFrame()
{
	SDL_Event event;
	
	// Process events from SDL
	while (SDL_PollEvent(&event) != 0)
	{
		ProcessEvents(event);
	}
	// If the window is open, start a new imgui frame
	if (!mWindow->mMinimized)
	{
		mGUI->NewFrame();
	}
}

void App::LoadModels()
{
	auto commandList = mGraphics->mCommandList.Get();

	// Multiple meshes, full PBR textured per mesh
	Model* boatModel = new Model("Models/Boat1.fbx", commandList);

	boatModel->SetPosition(XMFLOAT3{ -20.0f, 0.0f, 0.0f });
	boatModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	boatModel->SetScale(XMFLOAT3{ 0.1f, 0.1f, 0.1f });
	mModels.push_back(boatModel);

	//Model* roboModel = new Model("Models/snow2.fbx", commandList);

	//roboModel->SetPosition(XMFLOAT3{ -6.0f, 0.0f, 0.0f });
	//roboModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	//roboModel->SetScale(XMFLOAT3{ 0.005f, 0.005f, 0.005f });
	//mModels.push_back(roboModel);

	//Model* bladeModel = new Model("Models/robo_bun.fbx", commandList);

	//bladeModel->SetPosition(XMFLOAT3{ -14.0f, 0.0f, 0.0f });
	//bladeModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	//bladeModel->SetScale(XMFLOAT3{ 0.01f, 0.01f, 0.01f });
	//mModels.push_back(bladeModel);

	Model* plasmaModel = new Model("Models/plasmarifle.fbx", commandList);

	plasmaModel->SetPosition(XMFLOAT3{ -16.0f, 0.0f, 0.0f });
	plasmaModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	plasmaModel->SetScale(XMFLOAT3{ 0.01f, 0.01f, 0.01f });
	mModels.push_back(plasmaModel);

	// PBR per model texture display model
	Model* octoModel = new Model("Models/octopus.x", commandList, nullptr, "pjemy");

	octoModel->SetPosition(XMFLOAT3{ -10.0f, 0.0f, 0.0f });
	octoModel->SetRotation(XMFLOAT3{ -1.2f, 0.0f, 0.0f });
	octoModel->SetScale(XMFLOAT3{ 0.5f, 0.5f, 0.5f });
	mModels.push_back(octoModel);

	// Base material colour models
	Model* foxModel = new Model("Models/polyfox.fbx", commandList);

	foxModel->SetPosition(XMFLOAT3{ 4.0f, 0.0f, 0.0f });
	foxModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	foxModel->SetScale(XMFLOAT3{ 0.01f, 0.01f, 0.01f });
	mModels.push_back(foxModel);

	Model* wolfModel = new Model("Models/Wolf.fbx", commandList);

	wolfModel->SetPosition(XMFLOAT3{ 6.0f, 0.0f, 0.0f });
	wolfModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	wolfModel->SetScale(XMFLOAT3{ 0.01f, 0.01f, 0.01f });

	mModels.push_back(wolfModel);

	Model* slimeModel = new Model("Models/PolyFrog.fbx", commandList);

	slimeModel->SetPosition(XMFLOAT3{ 8.0f, 0.0f, 0.0f });
	slimeModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	slimeModel->SetScale(XMFLOAT3{ 0.002f, 0.002f, 0.002f });

	mModels.push_back(slimeModel);

	// Sort models by PSO
	int index = 0;
	for (auto& model : mModels)
	{
		model->mObjConstantBufferIndex = index;

		if (model->mTextured)
		{
			if (model->mPerMeshTextured)
			{
				if (model->mPerMeshPBR)
				{
					mTexModels.push_back(model);
				}
				else
				{
					mSimpleTexModels.push_back(model);
				}
			}
			else if (model->mModelTextured)
			{
				mTexModels.push_back(model);
			}
		}
		else
		{
			if(model != mPlanetModel) mColourModels.push_back(model);
		}		
		index++;
	}

	// Water and sky are rendered independently

	// Water 
	mWaterModel = new Model("Models/Water.fbx", commandList);
	 
	mWaterModel->SetPosition(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	mWaterModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	mWaterModel->SetScale(XMFLOAT3{ 0.01, 0.01, 0.01 });
	mWaterModel->mObjConstantBufferIndex = index;
	index++;

	mModels.push_back(mWaterModel);

	// Skybox
	mSkyModel = new Model("Models/sphere.x", commandList);

	mSkyModel->SetPosition(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	mSkyModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	mSkyModel->SetScale(XMFLOAT3{ 1, 1, 1 });
	mSkyModel->mObjConstantBufferIndex = index;

	mModels.push_back(mSkyModel);

}

void App::BuildFrameResources()
{
	for (int i = 0; i < mGraphics->mNumFrameResources; i++)
	{
		// Create a frame resource with the number of models, max base planet vertices and indices, and the number of materials 
		FrameResources.push_back(std::make_unique<FrameResource>(D3DDevice.Get(), 1, mModels.size(), MAX_PLANET_VERTS, MAX_PLANET_VERTS * 3, mMaterials.size())); //1 for planet
	}
}

void App::RecreatePlanetGeometry()
{
	// Create a new planet with the data from the GUI
	mPlanet->CreatePlanet(mGUI->mFrequency, mGUI->mOctaves, mGUI->mLOD, mPlanet->mScale, mGUI->mSeed);
	
	// Set the planet model's mesh to the new planet mesh and flag as dirty
	mModels[0]->mConstructorMesh = mPlanet->mMesh;
	mModels[0]->mNumDirtyFrames += mGraphics->mNumFrameResources;
}


void App::CreateSkybox()
{
	// Get device and create upload batch
	auto device = D3DDevice.Get();
	ResourceUploadBatch upload(device);
	upload.Begin();

	// Create new sky material
	mSkyMat = new Material();
	mSkyMat->DiffuseSRVIndex = CurrentSRVOffset;
	mSkyMat->Name = L"Models/4knebula.dds";

	// Create cube texture
	Texture* cubeTex = new Texture();
	DDS_ALPHA_MODE mode = DDS_ALPHA_MODE_OPAQUE;
	bool cubeMap = true;

	//CreateDDSTextureFromFileEx(D3DDevice.Get(), upload, mSkyMat->Name.c_str(), 0, D3D12_RESOURCE_FLAG_NONE,
	//	(DirectX::DX12::DDS_LOADER_FLAGS)(DirectX::DX12::DDS_LOADER_DEFAULT) | DirectX::DX12::DDS_LOADER_MIP_AUTOGEN,
	//	cubeTex->Resource.ReleaseAndGetAddressOf(), &mode, &cubeMap);

	// Load cube texture
	CreateDDSTextureFromFile(D3DDevice.Get(), upload, mSkyMat->Name.c_str(), cubeTex->Resource.ReleaseAndGetAddressOf(), true, 0Ui64, &mode, &cubeMap);

	// Offset to next descriptor
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(CurrentSRVOffset, CbvSrvUavDescriptorSize);

	// Get cube map resource
	auto cubeMapRes = cubeTex->Resource;

	// Create descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = cubeMapRes->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = cubeMapRes->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// Create SRV
	device->CreateShaderResourceView(cubeMapRes.Get(), &srvDesc, hDescriptor);

	// Upload the resources to the GPU.
	auto finish = upload.End(CommandQueue.Get());

	// Wait for the upload thread to terminate
	finish.wait();

	// Set skymodel material to sky material and push texture
	mSkyModel->mMeshes[0]->mMaterial = mSkyMat;
	mSkyModel->mMeshes[0]->mTextures.push_back(cubeTex);
	mSkyModel->mMeshes[0]->mMaterial->CBIndex = mCurrentMatCBIndex;
}

void App::UpdatePlanetBuffers()
{
	if (mModels[0]->mNumDirtyFrames > 0)
	{
		// Copy geometry into dynamic vertex and index buffers
		for (int i = 0; i < mPlanet->mMesh->mVertices.size(); i++)
		{
			mGraphics->mCurrentFrameResource->mPlanetVB->Copy(i, mPlanet->mMesh->mVertices[i]);
		}
		for (int i = 0; i < mPlanet->mMesh->mIndices.size(); i++)
		{
			mGraphics->mCurrentFrameResource->mPlanetIB->Copy(i, mPlanet->mMesh->mIndices[i]);
		}

		// Set the planet mesh's buffers 
		mModels[0]->mConstructorMesh->mGPUVertexBuffer = mGraphics->mCurrentFrameResource->mPlanetVB->GetBuffer();
		mModels[0]->mConstructorMesh->mGPUIndexBuffer = mGraphics->mCurrentFrameResource->mPlanetIB->GetBuffer();
		// NumDirtyFrames reduced by UpdatePerObject
	}
}

void App::Update(float frameTime)
{
	// Update GUI
	mGUI->UpdateModelData(mModels[mGUI->mSelectedModel]);
	mGUI->Update(mNumModels);

	// Update Camera
	mCamera->Update(frameTime, mGUI->mCameraOrbit, mGUI->mInvertY, mGUI->mSpeedMultipler);

	// Get inputs from the window
	if (mWindow->mScrollValue != 0) 
	{
		// Update camera speed when mouse wheel scrolled 
		mCamera->UpdateSpeed(mWindow->mScrollValue);
		mWindow->mScrollValue = 0;
	}
	// Camera movement
	if (mWindow->mForward)	mCamera->MoveForward();
	if (mWindow->mBackward) mCamera->MoveBackward();
	if (mWindow->mLeft)	mCamera->MoveLeft();
	if (mWindow->mRight) mCamera->MoveRight();
	if (mWindow->mUp)	mCamera->MoveUp();
	if (mWindow->mDown)	mCamera->MoveDown();

	// Reset command allocator before planet updates as meshes can be spawned
	auto commandList = mGraphics->mCommandList.Get();
	mGraphics->ResetCommandAllocator(mGraphics->mBaseCommandAllocators[CurrentFrameResourceIndex].Get());
	if (FAILED(commandList->Reset(mGraphics->mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr)))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	// Update planet
	if (mPlanet->Update(mCamera.get(), commandList))
	{
		// If planet geometry was updated set new mesh
		mModels[0]->mNumDirtyFrames += mGraphics->mNumFrameResources;
		mModels[0]->mConstructorMesh = mPlanet->mMesh;

		// Execute commands on command list
		mGraphics->CloseAndExecuteCommandList();

		// Reset command list
		commandList = mGraphics->mCommandList.Get();
		if (FAILED(commandList->Reset(mGraphics->mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr)))
		{
			MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
		}
	}

	// If planet's GUI values are updated
	if (mGUI->mPlanetUpdated)
	{
		// Set CLOD value to GUI's value
		mPlanet->mCLOD = mGUI->mCLOD;

		// Wait for GPU
		mGraphics->EmptyCommandQueue();

		// Recreate planet
		RecreatePlanetGeometry();

		// Execute commands
		mGraphics->CloseAndExecuteCommandList();
		
		// Wait for GPU
		mGraphics->EmptyCommandQueue();

		// Planet has been updated
		mGUI->mPlanetUpdated = false;
	}
	else
	{
		// No commands recorded
		mGraphics->CloseAndExecuteCommandList();
	}

	// Update model selected in GUI
	UpdateSelectedModel();

	// Update buffers
	UpdatePlanetBuffers();
	UpdatePerObjectConstantBuffers();
	UpdatePerFrameConstantBuffer();
	UpdatePerMaterialConstantBuffers();
}

void App::UpdateSelectedModel()
{
	// If the world matrix has changed
	if (mGUI->mWMatrixChanged)
	{
		// Set transform from GUI
		mModels[mGUI->mSelectedModel]->SetPosition(mGUI->mInPosition, false);
		mModels[mGUI->mSelectedModel]->SetRotation(mGUI->mInRotation, false);
		mModels[mGUI->mSelectedModel]->SetScale(mGUI->mInScale, true);
		mModels[mGUI->mSelectedModel]->mNumDirtyFrames += mGraphics->mNumFrameResources;

		mGUI->mWMatrixChanged = false;
	}
}

void App::UpdatePerObjectConstantBuffers()
{
	// Get reference to the current per object constant buffer
	auto currentObjectConstantBuffer = mGraphics->mCurrentFrameResource->mPerObjectConstantBuffer.get();
	
	for (auto& model : mModels)
	{
		// If their values have been changed
		if (model->mNumDirtyFrames > 0)
		{
			// Get the world matrix of the item
			XMMATRIX worldMatrix = XMLoadFloat4x4(&model->mWorldMatrix);
			bool parallax = model->mParallax;
			
			// Transpose data
			PerObjectConstants objectConstants;
			XMStoreFloat4x4(&objectConstants.WorldMatrix, XMMatrixTranspose(worldMatrix));
			objectConstants.parallax = parallax;

			// Copy the structure into the current buffer at the item's index
			currentObjectConstantBuffer->Copy(model->mObjConstantBufferIndex, objectConstants);

			model->mNumDirtyFrames--;
		}
	}
}

void App::UpdatePerFrameConstantBuffer()
{
	// Make a per frame constants structure
	PerFrameConstants perFrameConstantBuffer;

	// Create view, proj, and view proj matrices
	XMMATRIX view = XMLoadFloat4x4(&mCamera->mViewMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&mCamera->mProjectionMatrix);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	// Transpose them into the structure
	XMStoreFloat4x4(&perFrameConstantBuffer.ViewMatrix, XMMatrixTranspose(view));
	XMStoreFloat4x4(&perFrameConstantBuffer.ProjMatrix, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&perFrameConstantBuffer.ViewProjMatrix, XMMatrixTranspose(viewProj));

	// Set the rest of the structure's values
	perFrameConstantBuffer.EyePosW = mCamera->mPos;
	perFrameConstantBuffer.RenderTargetSize = XMFLOAT2((float)mWindow->mWidth, (float)mWindow->mHeight);
	perFrameConstantBuffer.NearZ = 1.0f;
	perFrameConstantBuffer.FarZ = 1000.0f;
	perFrameConstantBuffer.TotalTime = mTimer.GetTime();
	perFrameConstantBuffer.DeltaTime = mTimer.GetLapTime();
	perFrameConstantBuffer.AmbientLight = { 0.01f, 0.01f, 0.01f, 1.0f };
	perFrameConstantBuffer.TexDebugIndex = mGUI->mDebugTex;
	
	// Set light values
	XMVECTOR lightDir = -SphericalToCartesian(1.0f, mSunTheta, mSunPhi);
	XMStoreFloat3(&perFrameConstantBuffer.Lights[0].Direction, lightDir);

	perFrameConstantBuffer.Lights[0].Colour = { 0.5f,0.5f,0.5f };
	perFrameConstantBuffer.Lights[0].Position = { 4.0f, 4.0f, 0.0f };
	perFrameConstantBuffer.Lights[0].Direction = { mGUI->mLightDir[0], mGUI->mLightDir[1], mGUI->mLightDir[2] };
	perFrameConstantBuffer.Lights[0].Strength = { 2,2,2 };

	// Copy the structure into the per frame constant buffer
	auto currentFrameCB = mGraphics->mCurrentFrameResource->mPerFrameConstantBuffer.get();
	currentFrameCB->Copy(0, perFrameConstantBuffer);
}

void App::UpdatePerMaterialConstantBuffers()
{
	auto currMaterialCB = mGraphics->mCurrentFrameResource->mPerMaterialConstantBuffer.get();

	for (auto& mat : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed. If the cbuffer data changes, it needs to be updated for each FrameResource.
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			PerMaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			matConstants.Metallic = mat->Metalness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->Copy(mat->CBIndex, matConstants);

			// Next FrameResource needs to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void App::Draw(float frameTime)
{
	// Reset command allocator and create new list on it
	mGraphics->ResetCommandAllocator(0);
	auto commandList = mGraphics->StartCommandList(0, 0);

	mGraphics->SetViewportAndScissorRects(commandList);

	// Clear the back buffer and depth buffer.
	mGraphics->ClearBackBuffer(commandList);
	mGraphics->ClearDepthBuffer(commandList);

	// Select MSAA texture as render target
	mGraphics->SetMSAARenderTarget(commandList);

	mGraphics->SetDescriptorHeapsAndRootSignature(0, 0);

	// Set SRV heap
	auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

	// Set per-frame buffer
	auto perFrameBuffer = mGraphics->mCurrentFrameResource->mPerFrameConstantBuffer->GetBuffer();
	commandList->SetGraphicsRootConstantBufferView(2, perFrameBuffer->GetGPUVirtualAddress());

	// Set skybox texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE cubeTex(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
	cubeTex.Offset(mSkyMat->DiffuseSRVIndex, CbvSrvUavDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(4, cubeTex);
	
	// Draw models
	DrawModels(commandList);

	// Draw base planet geometry
	DrawPlanet(commandList);

	// Execute commands
	mGraphics->CloseAndExecuteCommandList(0, 0);

	// Thread planet chunk rendering
	int start = 0;
	int count = (mPlanet->mTriangleChunks.size() + mNumRenderWorkers - 1) / mNumRenderWorkers;
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		// Prepare work
		auto& work = mRenderWorkers[i].second;
		work.start = start;
		start += count;
		if (start > mPlanet->mTriangleChunks.size())  start = mPlanet->mTriangleChunks.size();
		work.end = start;

		// Flag the work as not yet complete
		auto& workerThread = mRenderWorkers[i].first;
		{
			// Mutex work complete
			std::unique_lock<std::mutex> l(workerThread.lock);
			work.complete = false;
		}

		// Signal the worker to start work
		workerThread.workReady.notify_one();
	}

	// Wait for each worker to finish
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		auto& workerThread = mRenderWorkers[i].first;
		auto& work = mRenderWorkers[i].second;

		// Wait for work completed signal
		std::unique_lock<std::mutex> l(workerThread.lock);
		workerThread.workReady.wait(l, [&]() { return work.complete; });
	}

	// Start a new command list
	commandList = mGraphics->StartCommandList(0, 1);

	// Setup command list
	mGraphics->SetDescriptorHeapsAndRootSignature(0, 1);
	mGraphics->SetViewportAndScissorRects(commandList);
	mGraphics->SetMSAARenderTarget(commandList);

	// Set transparent pipeline state for water 
	if (mWireframe) commandList->SetPipelineState(mGraphics->mWireframePSO.Get());
	else commandList->SetPipelineState(mGraphics->mWaterPSO.Get());

	mWaterModel->Draw(commandList);

	// Set skybox pipeline state for sky 
	commandList->SetPipelineState(mGraphics->mSkyPSO.Get());
	mSkyModel->Draw(commandList);

	// Resolve MSAA render target to actual back buffer
	mGraphics->ResolveMSAAToBackBuffer(commandList);

	// Render the GUI
	mGUI->Render(commandList, mGraphics->CurrentBackBuffer(), mGraphics->CurrentBackBufferView(), mGraphics->mDSVHeap.Get(), mGraphics->mDsvDescriptorSize);

	// Execute commands
	mGraphics->CloseAndExecuteCommandList(0, 1);

	// Swap back buffers with GUI vsync option
	mGraphics->SwapBackBuffers(mGUI->mVSync);
}

void App::DrawPlanet(ID3D12GraphicsCommandList* commandList)
{
	// Set pipeline state
	if (mWireframe) { commandList->SetPipelineState(mGraphics->mWireframePSO.Get()); }
	else { commandList->SetPipelineState(mGraphics->mPlanetPSO.Get()); }

	// Get size of the per object constant buffer 
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(PerObjectConstants));

	// Get reference to current per object constant buffer and set in command list
	auto objectCB = mGraphics->mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();
	auto objCBAddress = objectCB->GetGPUVirtualAddress(); // Planet is first in buffer
	commandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

	// Draw base planet geometry
	mModels[0]->mConstructorMesh->Draw(commandList);

	// Chunks are multithreaded
	/*for (auto& chunk : mPlanet->mTriangleChunks)
	{
		chunk->mMesh->Draw(commandList);
	}*/
}

void App::DrawModels(ID3D12GraphicsCommandList* commandList)
{
	// Set the pipeline state for each type of model and draw
	if (mWireframe) { commandList->SetPipelineState(mGraphics->mWireframePSO.Get()); }
	else { commandList->SetPipelineState(mGraphics->mSolidPSO.Get()); }

	for(int i = 0; i < mColourModels.size(); i++)
	{		
		mColourModels[i]->Draw(commandList);
	}

	if (mWireframe) { commandList->SetPipelineState(mGraphics->mWireframePSO.Get()); }
	else { commandList->SetPipelineState(mGraphics->mTexPSO.Get()); }

	for(int i = 0; i < mTexModels.size(); i++)
	{
		mTexModels[i]->Draw(commandList);
	}

	if (mWireframe) { commandList->SetPipelineState(mGraphics->mWireframePSO.Get()); }
	else { commandList->SetPipelineState(mGraphics->mSimpleTexPSO.Get()); }

	for (int i = 0; i < mSimpleTexModels.size(); i++)
	{
		mSimpleTexModels[i]->Draw(commandList);
	}
}

void App::RenderThread(int thread)
{
	auto& worker = mRenderWorkers[thread].first;
	auto& work = mRenderWorkers[thread].second;
	while (true)
	{
		{	// Mutex work complete
			std::unique_lock<std::mutex> l(worker.lock);

			// Wait for a signal
			worker.workReady.wait(l, [&]() { return !work.complete; });
		}

		// Start work
		RenderChunks(thread + 1, work.start, work.end); // Add one for main thread

		{ 
			// Mutex work complete
			std::unique_lock<std::mutex> l(worker.lock);
			work.complete = true; // Flag work as complete
		}

		// Signal work completed to main
		worker.workReady.notify_one();
	}
}

void App::RenderChunks(int thread, int start, int end)
{
	// Reset the main thread command allocator and start a new command lists on it
	mGraphics->ResetCommandAllocator(thread);
	auto commandList = mGraphics->StartCommandList(thread, 0);

	// Setup command list
	mGraphics->SetDescriptorHeapsAndRootSignature(thread, 0);

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(mGraphics->GetBackbufferWidth()), static_cast<float>(mGraphics->GetBackbufferHeight()), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT     scissorRect = { 0,    0,  static_cast<LONG> (mGraphics->GetBackbufferWidth()), static_cast<LONG> (mGraphics->GetBackbufferWidth()) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	mGraphics->SetMSAARenderTarget(commandList);

	// Set pipeline state to render planet
	if(mWireframe) commandList->SetPipelineState(mGraphics->mWireframePSO.Get());
	else commandList->SetPipelineState(mGraphics->mPlanetPSO.Get());

	// Render section of chunks
	for (int i = start; i < end; ++i)
		mPlanet->mTriangleChunks[i]->mMesh->Draw(commandList);

	// Execute commands
	mGraphics->CloseAndExecuteCommandList(thread, 0);
}

void App::EndFrame()
{
	// Advance fence value
	mGraphics->mCurrentFrameResource->Fence = ++mGraphics->mCurrentFence;

	// Set a new fence point when reached by GPU
	CommandQueue->Signal(mGraphics->mFence.Get(), mGraphics->mCurrentFence);

	// Cycle through frame resources
	mGraphics->CycleFrameResources();
}

void App::ProcessEvents(SDL_Event& event)
{
	// Let GUI process events
	if(mGUI->ProcessEvents(event)) return;
	
	// Process events from window
	mWindow->ProcessEvents(event);

	// Stop timer when minimised
	if (mWindow->mMinimized) mTimer.Stop();
	else mTimer.Start();

	// If window resized
	if (mWindow->mResized)
	{
		mGraphics->Resize(mWindow->mWidth, mWindow->mHeight);
		mCamera->WindowResized(mWindow.get());
		mWindow->mResized = false;
	}

	mWireframe = mWindow->mWireframe;

	// Capture mouse movement
	if (mWindow->mMouseMoved)
	{
		mCamera->MouseMoved(event,mWindow.get());
		mWindow->mMouseMoved = false;
	}
}

void App::CreateMaterials()
{
	// Index materials and add to list
	mCurrentMatCBIndex = 0;
	for (auto& model : mModels)
	{
		for (auto& mesh : model->mMeshes)
		{
			mesh->mMaterial->CBIndex = mCurrentMatCBIndex;
			mMaterials.push_back(mesh->mMaterial);
			mCurrentMatCBIndex++;
		}
	}

}

App::~App()
{
	// Empty the command queue
	if (D3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }

	// Close worker threads
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		mRenderWorkers[i].first.thread.detach();
	}

	for (auto& model : mModels)
	{
		delete model;
	}

}