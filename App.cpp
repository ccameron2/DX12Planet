#include "App.h"
#include "DDSTextureLoader.h"

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
	CycleFrameResources();

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

	mPlanet = std::make_unique<Planet>();
	mPlanet->ObjConstantBufferIndex = 0;
	XMStoreFloat4x4(&mPlanet->WorldMatrix, XMMatrixIdentity() * /*XMMatrixScaling(0, 0, 0) **/ XMMatrixTranslation(0.0f, 0.0f, 0.0f));

	LoadModels();
	CreateMaterials();

	BuildFrameResources();

	mNumModels = 1 + mModels.size(); // +1 for planet

	mSRVDescriptorHeap = make_unique<SRVDescriptorHeap>(mGraphics->mD3DDevice.Get(), mGraphics->mCbvSrvUavDescriptorSize);
	
	CreateTextures();
	CreateRootSignature();

	CreateShaders();
	CreatePSO();

	mGraphics->ExecuteCommands();

	mGUI = make_unique<GUI>(mSRVDescriptorHeap.get(), mWindow->mSDLWindow, mGraphics->mD3DDevice.Get(),
								mNumFrameResources, mGraphics->mBackBufferFormat);
}
void App::CreateTextures()
{
	//mMaterials.push_back(new Material());
	//mMaterials[0]->Name = L"Models/blocksrough";
	
	vector<Material*> materials;

	materials.push_back(new Material());
	materials[0]->Name = L"Models/blocksrough";

	mTextures.push_back(new Texture());
	mTextures[0]->Path = materials[0]->Name + L"-albedo.dds";

	auto device = mGraphics->mD3DDevice.Get();
	ResourceUploadBatch upload(device);

	upload.Begin();

	CreateDDSTextureFromFile(device,upload, mTextures[0]->Path.c_str(), mTextures[0]->Resource.ReleaseAndGetAddressOf(), false);
	
	// Fill out the heap with actual descriptors.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSRVDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
	// next descriptor
	hDescriptor.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

	auto foxTex = mTextures[0]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = foxTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = foxTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(foxTex.Get(), &srvDesc, hDescriptor);

	mTextures.push_back(new Texture());
	mTextures[1]->Path = materials[0]->Name + L"-roughness.dds";

	CreateDDSTextureFromFile(device, upload, mTextures[1]->Path.c_str(), mTextures[1]->Resource.ReleaseAndGetAddressOf(), false);

	hDescriptor.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

	auto foxRough = mTextures[1]->Resource;

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = foxRough->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = foxRough->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(foxRough.Get(), &srvDesc, hDescriptor);

	mTextures.push_back(new Texture());
	mTextures[2]->Path = materials[0]->Name + L"-metalness.dds";

	CreateDDSTextureFromFile(device, upload, mTextures[2]->Path.c_str(), mTextures[2]->Resource.ReleaseAndGetAddressOf(), false);

	hDescriptor.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

	auto foxMetal = mTextures[2]->Resource;

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = foxMetal->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = foxMetal->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(foxMetal.Get(), &srvDesc, hDescriptor);

	mTextures.push_back(new Texture());
	mTextures[3]->Path = materials[0]->Name + L"-normal.dds";

	//CreateWICTextureFromFile(device, upload, L"Models/subway-floor-normal.jpg", mTextures[1]->Resource.ReleaseAndGetAddressOf(), false);
	CreateDDSTextureFromFile(device, upload, mTextures[3]->Path.c_str(), mTextures[3]->Resource.ReleaseAndGetAddressOf(), false);

	hDescriptor.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

	auto foxNorm = mTextures[3]->Resource;

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = foxNorm->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = foxNorm->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(foxNorm.Get(), &srvDesc, hDescriptor);

	mTextures.push_back(new Texture());
	mTextures[4]->Path = materials[0]->Name + L"-height.dds";

	CreateDDSTextureFromFile(device, upload, mTextures[4]->Path.c_str(), mTextures[4]->Resource.ReleaseAndGetAddressOf(), false);

	hDescriptor.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

	auto foxDisplacement = mTextures[4]->Resource;

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = foxDisplacement->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = foxDisplacement->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(foxDisplacement.Get(), &srvDesc, hDescriptor);

	mTextures.push_back(new Texture());
	mTextures[5]->Path = materials[0]->Name + L"-ao.dds";

	CreateDDSTextureFromFile(device, upload, mTextures[5]->Path.c_str(), mTextures[5]->Resource.ReleaseAndGetAddressOf(), false);

	hDescriptor.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

	auto foxAO = mTextures[5]->Resource;

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = foxAO->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = foxAO->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(foxAO.Get(), &srvDesc, hDescriptor);

	// Upload the resources to the GPU.
	auto finish = upload.End(mGraphics->mCommandQueue.Get());

	// Wait for the upload thread to terminate
	finish.wait();
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
	Model* wolfModel = new Model("Models/Wolf.fbx", mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	XMStoreFloat4x4(&wolfModel->mWorldMatrix, XMMatrixIdentity()
												* XMMatrixScaling(1, 1, 1)
												* XMMatrixRotationX(90)
												* XMMatrixTranslation(8.0f, 0.0f, 0.0f));
	mModels.push_back(wolfModel);

	Model* foxModel = new Model("Models/polyfox.fbx", mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	XMStoreFloat4x4(&foxModel->mWorldMatrix, XMMatrixIdentity() 
												* XMMatrixScaling(1, 1, 1)
												* XMMatrixRotationX(90)
												* XMMatrixTranslation(4.0f, 0.0f, 0.0f));														
	mModels.push_back(foxModel);

	Model* anotherModel = new Model("Models/Slime.fbx", mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	XMStoreFloat4x4(&anotherModel->mWorldMatrix, XMMatrixIdentity()
												* XMMatrixScaling(1, 1, 1)
												* XMMatrixRotationX(90)
												* XMMatrixTranslation(-8.0f, 0.0f, 0.0f));
	mModels.push_back(anotherModel);

	Model* otherModel = new Model("Models/octopus.x", mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	XMStoreFloat4x4(&otherModel->mWorldMatrix, XMMatrixIdentity()
		* XMMatrixScaling(0.5, 0.5, 0.5)
		//* XMMatrixRotationX(90)
		* XMMatrixTranslation(-4.0f, 0.0f, 0.0f));
	mModels.push_back(otherModel);

	for (int i = 0; i < mModels.size(); i++)
	{
		mModels[i]->mObjConstantBufferIndex = 1 + i; // 1 for planet
	}
}

void App::BuildFrameResources()
{
	for (int i = 0; i < mNumFrameResources; i++)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mGraphics->mD3DDevice.Get(), 1, 1 + mModels.size(), 11000000, 65000000, mMaterials.size())); //1 for planet
	}
}

void App::CreateRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		6,  // number of descriptors
		0); // register t0

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = mGraphics->GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature
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

	if (FAILED(mGraphics->mD3DDevice->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf()))))
	{
		MessageBox(0, L"Root Signature creation failed", L"Error", MB_OK);
	}
}

ComPtr<ID3DBlob> App::CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
	UINT compileFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif`

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;

	if (FAILED(D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors)))
	{
		MessageBox(0, L"Shader compile failed", L"Error", MB_OK);
	}

	if (errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());

	return byteCode;
}

void App::CreateShaders()
{
	mColourVSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "VS", "vs_5_0");
	mColourPSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "PS", "ps_5_0");
	mColourInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mTexVSByteCode = CompileShader(L"Shaders\\texshader.hlsl", nullptr, "VS", "vs_5_0");
	mTexPSByteCode = CompileShader(L"Shaders\\texshader.hlsl", nullptr, "PS", "ps_5_0");
	mTexInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mPlanetVSByteCode = CompileShader(L"Shaders\\planetshader.hlsl", nullptr, "VS", "vs_5_0");
	mPlanetPSByteCode = CompileShader(L"Shaders\\planetshader.hlsl", nullptr, "PS", "ps_5_0");
}

void App::CreatePSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mColourInputLayout.data(), (UINT)mColourInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
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

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.MultisampleEnable = true;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mGraphics->mBackBufferFormat;
	psoDesc.SampleDesc.Count = mGraphics->mMSAASampleCount;
	psoDesc.SampleDesc.Quality = mGraphics->mMSAAQuality - 1;
	psoDesc.DSVFormat = mGraphics->mDepthStencilFormat;
	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mSolidPSO))))
	{
		MessageBox(0, L"Solid Pipeline State Creation failed", L"Error", MB_OK);
	}

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWireframePSO))))
	{
		MessageBox(0, L"Wireframe Pipeline State Creation failed", L"Error", MB_OK);
	}
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
	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWirePlanetPSO))))
	{
		MessageBox(0, L"Texture Pipeline State Creation failed", L"Error", MB_OK);
	}
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPlanetPSO))))
	{
		MessageBox(0, L"Texture Pipeline State Creation failed", L"Error", MB_OK);
	}
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mTexVSByteCode->GetBufferPointer()),
		mTexVSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mTexPSByteCode->GetBufferPointer()),
		mTexPSByteCode->GetBufferSize()
	};
	psoDesc.InputLayout = { mTexInputLayout.data(), (UINT)mTexInputLayout.size() };

	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mTexPSO))))
	{
		MessageBox(0, L"Texture Pipeline State Creation failed", L"Error", MB_OK);
	}
}

void App::RecreatePlanetGeometry()
{
	mPlanet->CreatePlanet(mGUI->mFrequency, mGUI->mOctaves, mGUI->mLOD);

	for (int i = 0; i < mPlanet->mVertices.size(); i++)
	{
		mCurrentFrameResource->mPlanetVB->Copy(i, mPlanet->mVertices[i]);
	}
	for (int i = 0; i < mPlanet->mIndices.size(); i++)
	{
		mCurrentFrameResource->mPlanetIB->Copy(i, mPlanet->mIndices[i]);
	}

	mPlanet->mMesh->mGPUVertexBuffer = mCurrentFrameResource->mPlanetVB->GetBuffer();
	mPlanet->mMesh->mGPUIndexBuffer = mCurrentFrameResource->mPlanetIB->GetBuffer();
}

void App::Update(float frameTime)
{
	mGUI->Update(mNumModels);

	mCamera->Update();

	// If the world matrix has changed
	if (mGUI->mWMatrixChanged)
	{
		// Create identity matrices
		XMMATRIX IDMatrix = XMMatrixIdentity();
		XMMATRIX translationMatrix = XMMatrixIdentity();

		// Apply transformations from GUI
		translationMatrix *= XMMatrixTranslation(mGUI->mPos[0], mGUI->mPos[1], mGUI->mPos[2]);
		
		translationMatrix *= XMMatrixRotationX(mGUI->mRot[0]) * XMMatrixRotationY(mGUI->mRot[1]) * XMMatrixRotationZ(mGUI->mRot[2]);
		
		translationMatrix *= XMMatrixScaling(mGUI->mScale, mGUI->mScale, mGUI->mScale);

		// Set result to icosahedron's world matrix
		XMStoreFloat4x4(&mGUIWorldMatrix, IDMatrix *= translationMatrix);

		// Set render item to gui world matrix
		if (mGUI->mSelectedModel == 0)
		{
			mPlanet->WorldMatrix = mGUIWorldMatrix;
			mPlanet->NumDirtyFrames += mNumFrameResources;
		}
		else
		{
			mModels[mGUI->mSelectedModel - 1]->mWorldMatrix = mGUIWorldMatrix;
			mModels[mGUI->mSelectedModel - 1]->mNumDirtyFrames += mNumFrameResources;
		}

		mGUI->mWMatrixChanged = false;
	}

	if (mGUI->mUpdated)
	{
		RecreatePlanetGeometry();
		mGUI->mUpdated = false;
	}

	UpdatePerObjectConstantBuffers();
	UpdatePerFrameConstantBuffer();
	UpdatePerMaterialConstantBuffers();
}

void App::CycleFrameResources()
{
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
}

void App::UpdatePerObjectConstantBuffers()
{
	// Get reference to the current per object constant buffer
	auto currentObjectConstantBuffer = mCurrentFrameResource->mPerObjectConstantBuffer.get();
	
	// If planet's values have been changed
	if (mPlanet->NumDirtyFrames > 0)
	{
		// Get the world matrix of the item
		XMMATRIX worldMatrix = XMLoadFloat4x4(&mPlanet->WorldMatrix);

		// Create a per object constants structure
		FrameResource::mPerObjectConstants objectConstants;

		// Transpose the world matrix into it
		XMStoreFloat4x4(&objectConstants.WorldMatrix, XMMatrixTranspose(worldMatrix));

		// Copy the structure into the current buffer at the item's index
		currentObjectConstantBuffer->Copy(mPlanet->ObjConstantBufferIndex, objectConstants);
		
		mPlanet->NumDirtyFrames--;
	}

	for (auto& model : mModels)
	{
		// If their values have been changed
		if (model->mNumDirtyFrames > 0)
		{
			// Get the world matrix of the item
			XMMATRIX worldMatrix = XMLoadFloat4x4(&model->mWorldMatrix);

			// Create a per object constants structure
			FrameResource::mPerObjectConstants objectConstants;

			// Transpose the world matrix into it
			XMStoreFloat4x4(&objectConstants.WorldMatrix, XMMatrixTranspose(worldMatrix));

			// Copy the structure into the current buffer at the item's index
			currentObjectConstantBuffer->Copy(model->mObjConstantBufferIndex, objectConstants);

			model->mNumDirtyFrames--;
		}
	}
}

void App::UpdatePerFrameConstantBuffer()
{
	// Make a per frame constants structure
	FrameResource::mPerFrameConstants perFrameConstantBuffer;

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
	perFrameConstantBuffer.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	XMVECTOR lightDir = -SphericalToCartesian(1.0f, mSunTheta, mSunPhi);
	XMStoreFloat3(&perFrameConstantBuffer.Lights[0].Direction, lightDir);

	perFrameConstantBuffer.Lights[0].Colour = { 0.5f,0.5f,0.5f };
	perFrameConstantBuffer.Lights[0].Position = { 4.0f, 4.0f, 0.0f };
	perFrameConstantBuffer.Lights[0].Direction = { mGUI->mLightDir[0], mGUI->mLightDir[1], mGUI->mLightDir[2] };
	perFrameConstantBuffer.Lights[0].Strength = { 0.5,0.5,0.5 };

	// Copy the structure into the per frame constant buffer
	auto currentFrameCB = mCurrentFrameResource->mPerFrameConstantBuffer.get();
	currentFrameCB->Copy(0, perFrameConstantBuffer);
}

void App::UpdatePerMaterialConstantBuffers()
{
	auto currMaterialCB = mCurrentFrameResource->mPerMaterialConstantBuffer.get();

	for (auto& mat : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->Copy(mat->CBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void App::Draw(float frameTime)
{
	auto commandAllocator = mCurrentFrameResource->mCommandAllocator.Get();
	auto commandList = mGraphics->mCommandList;

	mGraphics->ResetCommandAllocator(commandAllocator);

	mGraphics->ResetCommandList(commandAllocator, mCurrentPSO);

	mGraphics->SetViewportAndScissorRects();

	// Clear the back buffer and depth buffer.
	mGraphics->ClearBackBuffer();
	mGraphics->ClearDepthBuffer();

	// Select MSAA texture as render target
	mGraphics->SetMSAARenderTarget();

	mGraphics->SetDescriptorHeap(mSRVDescriptorHeap->mHeap.Get());

	commandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSRVDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

	auto perFrameBuffer = mCurrentFrameResource->mPerFrameConstantBuffer->GetBuffer();
	commandList->SetGraphicsRootConstantBufferView(2, perFrameBuffer->GetGPUVirtualAddress());

	DrawPlanet(commandList.Get());

	DrawModels(commandList.Get());

	mGraphics->ResolveMSAAToBackBuffer();

	mGUI->Render(mGraphics->mCommandList.Get(), mGraphics->CurrentBackBuffer(), mGraphics->CurrentBackBufferView(), mGraphics->mDSVHeap.Get(), mGraphics->mDsvDescriptorSize);

	mGraphics->CloseAndExecuteCommandList();

	mGraphics->SwapBackBuffers(mGUI->mVSync);
}

void App::DrawPlanet(ID3D12GraphicsCommandList* commandList)
{
	if (mWireframe) { commandList->SetPipelineState(mWirePlanetPSO.Get()); }
	else { commandList->SetPipelineState(mPlanetPSO.Get()); }
	// Get size of the per object constant buffer 
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));

	// Get reference to current per object constant buffer
	auto objectCB = mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();

	auto objCBAddress = objectCB->GetGPUVirtualAddress(); // Planet is first in buffer
	commandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

	mPlanet->mMesh->Draw(commandList);
}

void App::DrawModels(ID3D12GraphicsCommandList* commandList)
{
	if (mWireframe) { commandList->SetPipelineState(mWireframePSO.Get()); }
	else { commandList->SetPipelineState(mSolidPSO.Get()); }
	// Get size of the per object constant buffer 
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));

	// Get reference to current per object constant buffer
	auto objectCB = mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();
	auto matCB = mCurrentFrameResource->mPerMaterialConstantBuffer->GetBuffer();

	for(int i = 0; i < mModels.size(); i++)
	{		
		// Offset to the CBV for this object
		auto objCBAddress = objectCB->GetGPUVirtualAddress() + mModels[i]->mObjConstantBufferIndex * objCBByteSize;
		commandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

		// last model has textures
		if (i == mModels.size() - 1)
		{
			CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSRVDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
			tex.Offset(1, mGraphics->mCbvSrvUavDescriptorSize);

			if(!mWireframe){ commandList->SetPipelineState(mTexPSO.Get()); }
			else{ commandList->SetPipelineState(mWireframePSO.Get()); }

			commandList->SetGraphicsRootDescriptorTable(0, tex);
		}
		mModels[i]->Draw(commandList,matCB);
	}
}

void App::EndFrame()
{
	// Advance fence value
	mCurrentFrameResource->Fence = ++mGraphics->mCurrentFence;

	// Tell command queue to set new fence point, will only be set when the GPU gets to new fence value.
	mGraphics->mCommandQueue->Signal(mGraphics->mFence.Get(), mGraphics->mCurrentFence);

	CycleFrameResources();
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

	if(mWindow->mMiddleMouse)
	{
		mWireframe = !mWireframe;
	}

	if (mWindow->mMouseMoved)
	{
		mCamera->MouseMoved(event,mWindow.get());
		mWindow->mMouseMoved = false;
	}

}

void App::CreateMaterials()
{
	int index = 0;
	for (auto& model : mModels)
	{
		for (auto& material : model->mBaseMaterials)
		{
			material->CBIndex = index;
			mMaterials.push_back(material);
			index++;
		}
	}

}

App::~App()
{
	// Empty the command queue
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }

	//for (auto& material : mMaterials)
	//{
	//	delete material;
	//}

	for (auto& texture : mTextures)
	{
		delete texture;
	}

	for (auto& model : mModels)
	{
		delete model;
	}
}