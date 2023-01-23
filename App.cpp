#include "App.h"

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
	
	UpdateCamera();
	CreateIcosohedron();
	BuildSkullGeometry();
	CreateRenderItems();

	BuildFrameResources();

	mCBVDescriptorHeap = make_unique<DescriptorHeap>(mGraphics->mD3DDevice.Get(),mFrameResources,mNumRenderItems,mGraphics->mCbvSrvUavDescriptorSize);

	CreateRootSignature();
	CreateShaders();
	CreatePSO();

	mGraphics->ExecuteCommands();

	SetupGUI();

	// Fix for not being able to use GUI values before creating the GUI. Should be fixed when move to new planet class
	mIcosohedron->ResetGeometry(mEyePos, mGUI->mFrequency, mGUI->mRecursions, mGUI->mOctaves, mGUI->mTesselation);
	mIcosohedron->CreateGeometry();

	// Make initial projection matrix
	Resized();
}

void App::SetupGUI()
{
	mGUI = make_unique<GUI>();
	mGUI->SetupGUI(mCBVDescriptorHeap->mCBVHeap.Get(), mCBVDescriptorHeap->mGuiSrvOffset, mGraphics->mCbvSrvUavDescriptorSize, 
		mWindow->mSDLWindow, mGraphics->mD3DDevice.Get(), mNumFrameResources, mGraphics->mBackBufferFormat);
}

void App::CreateIcosohedron()
{
	if (!mIcosohedron) { mIcosohedron.release(); }
	mIcosohedron = std::make_unique<Icosahedron>(0.5, 1, 8, mEyePos, false);

	mPlanet = std::make_unique<Planet>(mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
}

void App::BuildSkullGeometry()
{
	std::ifstream fin("skull.txt");

	if (!fin)
	{
		MessageBox(0, L"skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
		vertices[i].Colour = { 0.9f,0.9f,0.9f,1.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	mSkullGeometry = make_unique<GeometryData>();
	D3DCreateBlob(vbByteSize, &mSkullGeometry->mCPUVertexBuffer);
	CopyMemory(mSkullGeometry->mCPUVertexBuffer->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &mSkullGeometry->mCPUIndexBuffer);
	CopyMemory(mSkullGeometry->mCPUIndexBuffer->GetBufferPointer(), indices.data(), ibByteSize);

	mSkullGeometry->mGPUVertexBuffer = CreateDefaultBuffer(vertices.data(), vbByteSize, mSkullGeometry->mVertexBufferUploader, mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());

	mSkullGeometry->mGPUIndexBuffer = CreateDefaultBuffer(indices.data(), ibByteSize, mSkullGeometry->mIndexBufferUploader, mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());

	mSkullGeometry->mVertexByteStride = sizeof(Vertex);
	mSkullGeometry->mVertexBufferByteSize = vbByteSize;
	mSkullGeometry->mIndexFormat = DXGI_FORMAT_R32_UINT;
	mSkullGeometry->mIndexBufferByteSize = ibByteSize;
	mSkullGeometry->mIndicesCount = (UINT)indices.size();
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
	XMStoreFloat4x4(&icoRenderItem->WorldMatrix, XMMatrixIdentity() * XMMatrixScaling(0, 0, 0) * XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	icoRenderItem->ObjConstantBufferIndex = 0;
	icoRenderItem->Geometry = mIcosohedron->mGeometryData.get();
	icoRenderItem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	icoRenderItem->IndexCount = mIcosohedron->mGeometryData->mIndices.size();
	icoRenderItem->StartIndexLocation = 0;
	icoRenderItem->BaseVertexLocation = 0;
	mRenderItems.push_back(icoRenderItem);

	RenderItem* planetRenderItem = new RenderItem();
	XMStoreFloat4x4(&planetRenderItem->WorldMatrix, XMMatrixIdentity() * /*XMMatrixScaling(0, 0, 0) **/ XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	planetRenderItem->ObjConstantBufferIndex = 1;
	planetRenderItem->Geometry = mPlanet->mGeometryData.get();
	planetRenderItem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	planetRenderItem->IndexCount = mPlanet->mGeometryData->mIndices.size();
	planetRenderItem->StartIndexLocation = 0;
	planetRenderItem->BaseVertexLocation = 0;
	mRenderItems.push_back(planetRenderItem);

	//RenderItem* skullRitem = new RenderItem();
	//skullRitem->WorldMatrix = MakeIdentity4x4();
	//skullRitem->ObjConstantBufferIndex = 2;
	//skullRitem->Geometry = mSkullGeometry.get();
	//skullRitem->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//skullRitem->IndexCount = mSkullGeometry->mIndicesCount;
	//skullRitem->StartIndexLocation = 0;
	//skullRitem->BaseVertexLocation = 0;
	//mRenderItems.push_back(skullRitem);
	
	mNumRenderItems = mRenderItems.size();
}

void App::BuildFrameResources()
{
	for (int i = 0; i < mNumFrameResources; i++)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mGraphics->mD3DDevice.Get(), 1, mNumRenderItems, 20000000, 80000000));
	}
}

void App::CreateRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
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
	mVSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "VS", "vs_5_0");
	mPSByteCode = CompileShader(L"Shaders\\shader.hlsl", nullptr, "PS", "ps_5_0");
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
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
}

void App::RecreateGeometry(bool tesselation)
{
	static bool firstFrame = true;
	if (!firstFrame)
	{
		mIcosohedron->ResetGeometry(mEyePos, mGUI->mFrequency, mGUI->mRecursions, mGUI->mOctaves, tesselation);
		mIcosohedron->CreateGeometry();
	}
	else { firstFrame = false; }

	for (int i = 0; i < mIcosohedron->mVertices.size(); i++)
	{
		mCurrentFrameResource->mPlanetVB->Copy(i, mIcosohedron->mVertices[i]);
	}
	for (int i = 0; i < mIcosohedron->mIndices.size(); i++)
	{
		mCurrentFrameResource->mPlanetIB->Copy(i, mIcosohedron->mIndices[i]);
	}

	mRenderItems[0]->Geometry->mGPUVertexBuffer = mCurrentFrameResource->mPlanetVB->GetBuffer();
	mRenderItems[0]->Geometry->mGPUIndexBuffer = mCurrentFrameResource->mPlanetIB->GetBuffer();
	mRenderItems[0]->IndexCount = mIcosohedron->mIndices.size();
}

bool Float3IsSame(XMFLOAT3 a, XMFLOAT3 b) 
{
	if (a.x == b.x && a.y == b.y && a.z == b.z) 
	{
		return true;
	}
	else
	{
		return false;
	}
}

void App::Update(float frameTime)
{
	mGUI->Update(mNumRenderItems);

	UpdateCamera();

	// Create geometry if its the first frame
	static bool firstGenerationFrame = true;
	if (firstGenerationFrame)
	{
		RecreateGeometry(mGUI->mTesselation);
		firstGenerationFrame = false;
	}

	// If tesselation is enabled recreate geometry
	if (mGUI->mTesselation)
	{
		if (!Float3IsSame(mLastEyePos,mEyePos)) 
		{
			RecreateGeometry(mGUI->mTesselation);
			mLastEyePos = mEyePos;
		}
	}

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

		// Set first render item to icosahedrons world matrix
		mRenderItems[mGUI->mSelectedRenderItem]->WorldMatrix = mGUIWorldMatrix;
		mRenderItems[mGUI->mSelectedRenderItem]->NumDirtyFrames += mNumFrameResources;

		mGUI->mWMatrixChanged = false;
	}

	if (mGUI->mUpdated)
	{
		RecreateGeometry(mGUI->mTesselation);
		mGUI->mUpdated = false;
	}

	UpdatePerObjectConstantBuffers();
	UpdatePerFrameConstantBuffer();
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
	
	// For each render item
	for (auto& rItem : mRenderItems)
	{
		// If their values have been changed
		if (rItem->NumDirtyFrames > 0)
		{
			// Get the world matrix of the item
			XMMATRIX worldMatrix = XMLoadFloat4x4(&rItem->WorldMatrix);

			// Create a per object constants structure
			FrameResource::mPerObjectConstants objectConstants;

			// Transpose the world matrix into it
			XMStoreFloat4x4(&objectConstants.WorldMatrix, XMMatrixTranspose(worldMatrix));

			// Copy the structure into the current buffer at the item's index
			currentObjectConstantBuffer->Copy(rItem->ObjConstantBufferIndex, objectConstants);
			
			rItem->NumDirtyFrames--;
		}
	}
}

void App::UpdatePerFrameConstantBuffer()
{
	// Make a per frame constants structure
	FrameResource::mPerFrameConstants perFrameConstantBuffer;

	// Create view, proj, and view proj matrices
	XMMATRIX view = XMLoadFloat4x4(&mViewMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&mProjectionMatrix);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	// Transpose them into the structure
	XMStoreFloat4x4(&perFrameConstantBuffer.ViewMatrix, XMMatrixTranspose(view));
	XMStoreFloat4x4(&perFrameConstantBuffer.ProjMatrix, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&perFrameConstantBuffer.ViewProjMatrix, XMMatrixTranspose(viewProj));

	// Set the rest of the structure's values
	perFrameConstantBuffer.EyePosW = mEyePos;
	perFrameConstantBuffer.RenderTargetSize = XMFLOAT2((float)mWindow->mWidth, (float)mWindow->mHeight);
	perFrameConstantBuffer.NearZ = 1.0f;
	perFrameConstantBuffer.FarZ = 1000.0f;
	perFrameConstantBuffer.TotalTime = mTimer.GetTime();
	perFrameConstantBuffer.DeltaTime = mTimer.GetLapTime();
	perFrameConstantBuffer.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	XMVECTOR lightDir = -SphericalToCartesian(1.0f, mSunTheta, mSunPhi);
	XMStoreFloat3(&perFrameConstantBuffer.Lights[0].Direction, lightDir);

	//perFrameConstantBuffer.Lights[0].Strength = { 2.0f, 2.0f, 2.0f };
	perFrameConstantBuffer.Lights[0].Position = { 0.0f, 0.0f, 8.0f };
	perFrameConstantBuffer.Lights[0].Direction = { -0.57735f, -0.57735f, 0.57735f };
	//perFrameConstantBuffer.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	perFrameConstantBuffer.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	//perFrameConstantBuffer.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	//perFrameConstantBuffer.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	//perFrameConstantBuffer.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	//perFrameConstantBuffer.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	 	
	// Copy the structure into the per frame constant buffer
	auto currentFrameCB = mCurrentFrameResource->mPerFrameConstantBuffer.get();
	currentFrameCB->Copy(0, perFrameConstantBuffer);
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
	auto commandAllocator = mCurrentFrameResource->mCommandAllocator.Get();
	auto commandList = mGraphics->mCommandList;

	mGraphics->ResetCommandAllocator(commandAllocator);

	if (mWireframe) { mCurrentPSO = mSolidPSO.Get(); }
	else { mCurrentPSO = mWireframePSO.Get(); }

	mGraphics->ResetCommandList(commandAllocator, mCurrentPSO);

	mGraphics->SetViewportAndScissorRects();

	// Clear the back buffer and depth buffer.
	mGraphics->ClearBackBuffer();
	mGraphics->ClearDepthBuffer();

	// Select MSAA texture as render target
	mGraphics->SetMSAARenderTarget();

	mGraphics->SetDescriptorHeap(mCBVDescriptorHeap->mCBVHeap.Get());

	commandList->SetGraphicsRootSignature(mRootSignature.Get());

	int frameCbvIndex = mCBVDescriptorHeap->mFrameCbvOffset + mCurrentFrameResourceIndex;
	mGraphics->SetGraphicsRootDescriptorTable(mCBVDescriptorHeap->mCBVHeap.Get(), frameCbvIndex,1);

	DrawRenderItems(commandList.Get());

	mGraphics->ResolveMSAAToBackBuffer();

	mGUI->Render(mGraphics->mCommandList.Get(), mGraphics->CurrentBackBuffer(), mGraphics->CurrentBackBufferView(), mGraphics->mDSVHeap.Get(), mGraphics->mDsvDescriptorSize);

	mGraphics->CloseAndExecuteCommandList();

	mGraphics->SwapBackBuffers(mGUI->mVSync);
}

void App::DrawRenderItems(ID3D12GraphicsCommandList* commandList)
{
	// Get size of the per object constant buffer 
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));

	// Get reference to current per object constant buffer
	auto objectCB = mCurrentFrameResource->mPerObjectConstantBuffer->GetBuffer();

	// For each render item
	for (size_t i = 0; i < mRenderItems.size(); ++i)
	{
		// Set topology, vertex and index buffers to render item's
		auto renderItem = mRenderItems[i];
		commandList->IASetVertexBuffers(0, 1, &renderItem->Geometry->GetVertexBufferView());
		commandList->IASetIndexBuffer(&renderItem->Geometry->GetIndexBufferView());
		commandList->IASetPrimitiveTopology(renderItem->Topology);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource
		UINT cbvIndex = mCurrentFrameResourceIndex * (UINT)mRenderItems.size()+ renderItem->ObjConstantBufferIndex;
		mGraphics->SetGraphicsRootDescriptorTable(mCBVDescriptorHeap->mCBVHeap.Get(), cbvIndex, 0);
		commandList->DrawIndexedInstanced(renderItem->IndexCount, 1, renderItem->StartIndexLocation, renderItem->BaseVertexLocation, 0);
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

void App::MouseMoved(SDL_Event& event)
{
	int mouseX, mouseY;
	mouseX = event.motion.x;
	mouseY = event.motion.y;
	if (mWindow->mLeftMouse)
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
	else if (mWindow->mRightMouse)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(mouseX - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(mouseY - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = std::clamp(mRadius, 0.1f, 30.0f);
	}
	mLastMousePos.x = mouseX;
	mLastMousePos.y = mouseY;

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
		Resized();
		mWindow->mResized = false;
	}

	if(mWindow->mMiddleMouse)
	{
		mWireframe = !mWireframe;
	}

	if (mWindow->mMouseMoved)
	{
		MouseMoved(event);
		mWindow->mMouseMoved = false;
	}

}

void App::Resized()
{
	// The window resized, so update the aspect ratio and recompute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(mWindow->mWidth) / mWindow->mHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProjectionMatrix, proj);
}

App::~App()
{
	// Empty the command queue
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }

	// Delete render items
	for (auto& renderItem : mRenderItems)
	{
		delete renderItem;
	}
}