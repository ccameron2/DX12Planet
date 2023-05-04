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
	mWindow = make_unique<Window>(800,600);

	HWND hwnd = mWindow->GetHWND();
;
	mGraphics = make_unique<Graphics>(hwnd, mWindow->mWidth, mWindow->mHeight);
	
	mCamera = make_unique<Camera>(mWindow.get());

	mCamera->Update();

	mPlanet = std::make_unique<Planet>(mGraphics.get());
	mPlanet->CreatePlanet(0.1, 1, 1, 1,rand());

	auto planetModel = new Model("", mGraphics->mCommandList.Get(), mPlanet->mMesh);
	planetModel->SetPosition(XMFLOAT3{ 0, 0, 0 },false);
	planetModel->SetRotation(XMFLOAT3{ 0, 0, 0 }, false);
	planetModel->SetScale(XMFLOAT3{ float(mPlanet->mScale), float(mPlanet->mScale), float(mPlanet->mScale) }, true);
	planetModel->mParallax = false;

	mModels.push_back(planetModel);

	LoadModels();

	CreateSkybox();

	CreateMaterials();

	BuildFrameResources();

	mNumModels = mModels.size();

	mGraphics->CreateRootSignature();

	mGraphics->CreateShaders();
	mGraphics->CreatePSO();

	mGraphics->ExecuteCommands();

	mGUI = make_unique<GUI>(SrvDescriptorHeap.get(), mWindow->mSDLWindow, D3DDevice.Get(),
		mGraphics->mNumFrameResources, mGraphics->mBackBufferFormat);

	//// Start worker threads

	mNumRenderWorkers = std::thread::hardware_concurrency(); // Gives a hint about level of thread concurrency supported by system (0 means no hint given)
	if (mNumRenderWorkers == 0)  mNumRenderWorkers = 8;
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		mRenderWorkers[i].first.thread = std::thread(&App::RenderThread, this, i);
	}

}

void App::CreateSkybox()
{
	auto device = D3DDevice.Get();
	ResourceUploadBatch upload(device);

	upload.Begin();

	mSkyMat = new Material();
	mSkyMat->DiffuseSRVIndex = CurrentSRVOffset;
	mSkyMat->Name = L"Models/4knebula.dds";

	Texture* cubeTex = new Texture();
	DDS_ALPHA_MODE mode = DDS_ALPHA_MODE_OPAQUE;
	bool cubeMap = true;

	//CreateDDSTextureFromFileEx(D3DDevice.Get(), upload, mSkyMat->Name.c_str(), 0, D3D12_RESOURCE_FLAG_NONE,
	//	(DirectX::DX12::DDS_LOADER_FLAGS)(DirectX::DX12::DDS_LOADER_DEFAULT) | DirectX::DX12::DDS_LOADER_MIP_AUTOGEN,
	//	cubeTex->Resource.ReleaseAndGetAddressOf(), &mode, &cubeMap);

	CreateDDSTextureFromFile(D3DDevice.Get(), upload, mSkyMat->Name.c_str(), cubeTex->Resource.ReleaseAndGetAddressOf(), true, 0Ui64, &mode, &cubeMap);

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
	// next descriptor
	hDescriptor.Offset(CurrentSRVOffset, CbvSrvUavDescriptorSize);

	auto cubeMapRes = cubeTex->Resource;
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = cubeMapRes->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = cubeMapRes->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(cubeMapRes.Get(), &srvDesc, hDescriptor);

	// Upload the resources to the GPU.
	auto finish = upload.End(CommandQueue.Get());

	// Wait for the upload thread to terminate
	finish.wait();

	mSkyModel->mMeshes[0]->mMaterial = mSkyMat;
	mSkyModel->mMeshes[0]->mTextures.push_back(cubeTex);
	mSkyModel->mMeshes[0]->mMaterial->CBIndex = mCurrentMatCBIndex;
}

void App::StartFrame()
{
	SDL_Event event;
	while (SDL_PollEvent(&event) != 0)
	{
		ProcessEvents(event);
	}
	if (!mWindow->mMinimized)
	{
		mGUI->NewFrame();
	}
}

void App::LoadModels()
{
	auto commandList = mGraphics->mCommandList.Get();

	Model* smgModel = new Model("Models/Boat1.fbx", commandList);

	smgModel->SetPosition(XMFLOAT3{ -20.0f, 0.0f, 0.0f });
	smgModel->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	smgModel->SetScale(XMFLOAT3{ 0.1f, 0.1f, 0.1f });
	mModels.push_back(smgModel);

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

	// Multiple meshes, full PBR textured per mesh

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

	//Model* octoModel2 = new Model("Models/octopus.x", commandList, octoModel->mMeshes[0], "pjemy");

	//octoModel2->SetPosition(XMFLOAT3{ -10.0f, 0.0f, 0.0f });
	//octoModel2->SetRotation(XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	//octoModel2->SetScale(XMFLOAT3{ 0.5, 0.5, 0.5 });
	//mModels.push_back(octoModel2);
	//mTexModels.push_back(octoModel2);

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
			mColourModels.push_back(model);
		}		
			index++;
	}

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
		FrameResources.push_back(std::make_unique<FrameResource>(D3DDevice.Get(), 1, mModels.size(), MAX_PLANET_VERTS, MAX_PLANET_VERTS * 3, mMaterials.size())); //1 for planet
	}
}

void App::RecreatePlanetGeometry()
{
	mPlanet->CreatePlanet(mGUI->mFrequency, mGUI->mOctaves, mGUI->mLOD, mPlanet->mScale, mGUI->mSeed);
	mModels[0]->mConstructorMesh = mPlanet->mMesh;
	mModels[0]->mNumDirtyFrames += mGraphics->mNumFrameResources;
}

void App::UpdatePlanetBuffers()
{
	if (mModels[0]->mNumDirtyFrames > 0)
	{
		for (int i = 0; i < mPlanet->mMesh->mVertices.size(); i++)
		{
			mGraphics->mCurrentFrameResource->mPlanetVB->Copy(i, mPlanet->mMesh->mVertices[i]);
		}
		for (int i = 0; i < mPlanet->mMesh->mIndices.size(); i++)
		{
			mGraphics->mCurrentFrameResource->mPlanetIB->Copy(i, mPlanet->mMesh->mIndices[i]);
		}

		mModels[0]->mConstructorMesh->mGPUVertexBuffer = mGraphics->mCurrentFrameResource->mPlanetVB->GetBuffer();
		mModels[0]->mConstructorMesh->mGPUIndexBuffer = mGraphics->mCurrentFrameResource->mPlanetIB->GetBuffer();
		// NumDirtyFrames reduced by UpdatePerObject
	}
}

void App::Update(float frameTime)
{
	mGUI->UpdateModelData(mModels[mGUI->mSelectedModel]);

	mGUI->Update(mNumModels);

	mCamera->Update(frameTime, mGUI->mCameraOrbit, mGUI->mInvertY, mGUI->mSpeedMultipler);

	if (mWindow->mScrollValue != 0) 
	{
		mCamera->UpdateSpeed(mWindow->mScrollValue);
		mWindow->mScrollValue = 0;
	}
	if (mWindow->mForward)	mCamera->MoveForward();
	if (mWindow->mBackward) mCamera->MoveBackward();
	if (mWindow->mLeft)	mCamera->MoveLeft();
	if (mWindow->mRight) mCamera->MoveRight();
	if (mWindow->mUp)	mCamera->MoveUp();
	if (mWindow->mDown)	mCamera->MoveDown();

	//Reset command allocator and list before planet updates as meshes can be spawned
	//auto commandAllocator = mGraphics->mCurrentFrameResource->mCommandAllocator.Get();
	//auto commandList = mGraphics->mCommandList;
	//mGraphics->ResetCommandAllocator(commandAllocator);
	//mGraphics->ResetCommandList(commandAllocator, mCurrentPSO);

	// Reset allocator and start new command list on it

	auto commandList = mGraphics->mCommandList.Get();
	mGraphics->ResetCommandAllocator(mGraphics->mBaseCommandAllocators[CurrentFrameResourceIndex].Get());
	if (FAILED(commandList->Reset(mGraphics->mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr)))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	if (mPlanet->Update(mCamera.get(), commandList))
	{
		mModels[0]->mNumDirtyFrames += mGraphics->mNumFrameResources;
		mModels[0]->mConstructorMesh = mPlanet->mMesh;

		mGraphics->CloseAndExecuteCommandList();
		//mGraphics->EmptyCommandQueue();

		commandList = mGraphics->mCommandList.Get();
		if (FAILED(commandList->Reset(mGraphics->mBaseCommandAllocators[CurrentFrameResourceIndex].Get(), nullptr)))
		{
			MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
		}
	}

	UpdateSelectedModel();

	if (mGUI->mPlanetUpdated)
	{
		mPlanet->mCLOD = mGUI->mCLOD;
		mGraphics->EmptyCommandQueue();

		RecreatePlanetGeometry();

		mGraphics->CloseAndExecuteCommandList();
		mGraphics->EmptyCommandQueue();

		mGUI->mPlanetUpdated = false;
	}
	else
	{
		// No commands recorded
		mGraphics->CloseAndExecuteCommandList();
	}

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
	mGraphics->ResetCommandAllocator(0);
	auto commandList = mGraphics->StartCommandList(0, 0);

	//mGraphics->ResetCommandAllocator(commandAllocator);

	//mGraphics->ResetCommandList(commandAllocator, mCurrentPSO);

	mGraphics->SetViewportAndScissorRects(commandList);

	// Clear the back buffer and depth buffer.
	mGraphics->ClearBackBuffer(commandList);
	mGraphics->ClearDepthBuffer(commandList);

	// Select MSAA texture as render target
	mGraphics->SetMSAARenderTarget(commandList);

	mGraphics->SetDescriptorHeapsAndRootSignature(0, 0);

	auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

	auto perFrameBuffer = mGraphics->mCurrentFrameResource->mPerFrameConstantBuffer->GetBuffer();
	commandList->SetGraphicsRootConstantBufferView(2, perFrameBuffer->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE cubeTex(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
	cubeTex.Offset(mSkyMat->DiffuseSRVIndex, CbvSrvUavDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(4, cubeTex);

	DrawModels(commandList);

	DrawPlanet(commandList);

	// All opening commands added to first main thread command list, now execute it
	mGraphics->CloseAndExecuteCommandList(0, 0);

	// Do Threading
	int start = 0;
	int count = (mPlanet->mTriangleChunks.size() + mNumRenderWorkers - 1) / mNumRenderWorkers;
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		// Prepare work parameters - a chunk of the boats
		auto& work = mRenderWorkers[i].second;
		work.start = start;
		start += count;
		if (start > mPlanet->mTriangleChunks.size())  start = mPlanet->mTriangleChunks.size();
		work.end = start;

		// Flag the work as not yet complete
		auto& workerThread = mRenderWorkers[i].first;
		{
			// Guard every access to shared variable "work.complete" with a mutex (see BlockSpritesThread comments)
			std::unique_lock<std::mutex> l(workerThread.lock);
			work.complete = false;
		}

		// Notify the worker thread via a condition variable - this will wake the worker thread up
		workerThread.workReady.notify_one();
	}

	// Wait for all the workers to finish
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		auto& workerThread = mRenderWorkers[i].first;
		auto& work = mRenderWorkers[i].second;

		// Wait for a signal via a condition variable indicating that the worker is complete
		// See comments in BlockSpritesThread regarding the mutex and the wait method
		std::unique_lock<std::mutex> l(workerThread.lock);
		workerThread.workReady.wait(l, [&]() { return work.complete; });
	}

	commandList = mGraphics->StartCommandList(0, 1);

	mGraphics->SetDescriptorHeapsAndRootSignature(0, 1);
	mGraphics->SetViewportAndScissorRects(commandList);
	mGraphics->SetMSAARenderTarget(commandList);

	if (mWireframe) commandList->SetPipelineState(mGraphics->mWireframePSO.Get());
	else commandList->SetPipelineState(mGraphics->mWaterPSO.Get());

	mWaterModel->Draw(commandList);

	commandList->SetPipelineState(mGraphics->mSkyPSO.Get());

	mSkyModel->Draw(commandList);

	mGraphics->ResolveMSAAToBackBuffer(commandList);

	mGUI->Render(commandList, mGraphics->CurrentBackBuffer(), mGraphics->CurrentBackBufferView(), mGraphics->mDSVHeap.Get(), mGraphics->mDsvDescriptorSize);

	// All commands added to post threading command list, now execute it
	mGraphics->CloseAndExecuteCommandList(0, 1);

	//mGraphics->CloseAndExecuteCommandList();

	mGraphics->SwapBackBuffers(mGUI->mVSync);
}

void App::DrawPlanet(ID3D12GraphicsCommandList* commandList)
{
	if (mWireframe) { commandList->SetPipelineState(mGraphics->mWireframePSO.Get()); }
	else { commandList->SetPipelineState(mGraphics->mPlanetPSO.Get()); }

	// Get size of the per object constant buffer 
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(PerObjectConstants));

	// Get reference to current per object constant buffer
	auto objectCB = mGraphics->mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();

	auto objCBAddress = objectCB->GetGPUVirtualAddress(); // Planet is first in buffer
	commandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

	mModels[0]->mConstructorMesh->Draw(commandList);
	
	/*for (auto& chunk : mPlanet->mTriangleChunks)
	{
		chunk->mMesh->Draw(commandList);
	}*/
}

void App::DrawModels(ID3D12GraphicsCommandList* commandList)
{

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

// Worker thread for rendering - exists throughout program, sleeps when waiting for work
void App::RenderThread(int thread)
{
	auto& worker = mRenderWorkers[thread].first;
	auto& work = mRenderWorkers[thread].second;
	while (true)
	{
		{ // Guard access (to work.complete)
			std::unique_lock<std::mutex> l(worker.lock);
			worker.workReady.wait(l, [&]() { return !work.complete; }); // Wait until a workReady signal arrives, then verify it by testing that work.complete is false
		}

		// We have some work so do it...
		RenderChunks(thread + 1, work.start, work.end); // Main thread counts as thread 0 so + 1

		{ // Guard access (to work.complete)
			std::unique_lock<std::mutex> l(worker.lock);
			work.complete = true; // Flag work as complete
		}
		// Send a signal back to the main thread to say the work is complete, loop back and wait for more work
		worker.workReady.notify_one();
	}
}

// Threaded work - render a lot of boats
void App::RenderChunks(int thread, int start, int end)
{
	// Reset the main thread command allocator and start a new command lists on it
	mGraphics->ResetCommandAllocator(thread);
	auto commandList = mGraphics->StartCommandList(thread, 0);

	mGraphics->SetDescriptorHeapsAndRootSignature(thread, 0);

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(mGraphics->GetBackbufferWidth()), static_cast<float>(mGraphics->GetBackbufferHeight()), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT     scissorRect = { 0,    0,  static_cast<LONG> (mGraphics->GetBackbufferWidth()), static_cast<LONG> (mGraphics->GetBackbufferWidth()) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	mGraphics->SetMSAARenderTarget(commandList);

	if(mWireframe) commandList->SetPipelineState(mGraphics->mWireframePSO.Get());
	else commandList->SetPipelineState(mGraphics->mPlanetPSO.Get());

	for (int i = start; i < end; ++i)
		mPlanet->mTriangleChunks[i]->mMesh->Draw(commandList);

	mGraphics->CloseAndExecuteCommandList(thread, 0);
}

void App::EndFrame()
{
	// Advance fence value
	mGraphics->mCurrentFrameResource->Fence = ++mGraphics->mCurrentFence;

	// Tell command queue to set new fence point, will only be set when the GPU gets to new fence value.
	CommandQueue->Signal(mGraphics->mFence.Get(), mGraphics->mCurrentFence);

	mGraphics->CycleFrameResources();
}

void App::ProcessEvents(SDL_Event& event)
{
	if(mGUI->ProcessEvents(event)) return;
	
	mWindow->ProcessEvents(event);

	if (mWindow->mMinimized) mTimer.Stop();
	else mTimer.Start();

	if (mWindow->mResized)
	{
		mGraphics->Resize(mWindow->mWidth, mWindow->mHeight);
		mCamera->WindowResized(mWindow.get());
		mWindow->mResized = false;
	}

	//if (mWindow->mLeftMouse)
	//{
	//	mTexModels[0]->mParallax = !mTexModels[0]->mParallax;
	//	mTexModels[0]->mNumDirtyFrames = mNumFrameResources;
	//}

	mWireframe = mWindow->mWireframe;

	if (mWindow->mMouseMoved)
	{
		mCamera->MouseMoved(event,mWindow.get());
		mWindow->mMouseMoved = false;
	}
}

void App::CreateMaterials()
{
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

	// Need to close off worker threads - they are in an infinite loop so cannot wait for them with join, so detach them - will be destroyed when app closes (should do better than this)
	for (int i = 0; i < mNumRenderWorkers; ++i)
	{
		mRenderWorkers[i].first.thread.detach();
	}

	for (auto& model : mModels)
	{
		delete model;
	}

	for (auto& texture : mTextures)
	{
		delete texture;
	}

}