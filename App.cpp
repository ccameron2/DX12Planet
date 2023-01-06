#include "App.h"
#include <sdl_syswm.h>

App::App()
{
	Initialize();

	// Set window text to title
	std::string windowText = mMainCaption;
	const char* array = windowText.c_str();
	SDL_SetWindowTitle(mWindow, array);

	Run();
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
		while (SDL_PollEvent(&event) != 0)
		{
			PollEvents(event);
		}
		if (!mMinimized)
		{
			// Start the ImGui frame
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

	mGraphics = make_unique<Graphics>(hwnd, mWidth, mHeight);
	
	UpdateCamera();
	CreateIcosohedron();
	BuildSkullGeometry();
	CreateRenderItems();

	BuildFrameResources();
	CreateCBVHeap();
	CreateConstantBuffers();
	CreateRootSignature();
	CreateShaders();
	CreatePSO();

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

	auto cpuhandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
	cpuhandle.Offset(mGUISRVOffset, mGraphics->mCbvSrvUavDescriptorSize);

	auto gpuhandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	gpuhandle.Offset(mGUISRVOffset, mGraphics->mCbvSrvUavDescriptorSize);

	ImGui_ImplSDL2_InitForD3D(mWindow);
	ImGui_ImplDX12_Init(mGraphics->mD3DDevice.Get(), mNumFrameResources, mGraphics->mBackBufferFormat, mCBVHeap.Get(),
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

	if (ImGui::SliderInt("Recursions", &mRecursions, 0, 10))
	{
		RecreateGeometry(mTesselation);
	};

	if (ImGui::Checkbox("Tesselation", &mTesselation))
	{
		RecreateGeometry(mTesselation);
	};

	ImGui::Text("Noise");

	if (ImGui::SliderFloat("Noise Frequency", &mFrequency, 0.0f, 1.0f, "%.1f"))
	{
		RecreateGeometry(mTesselation);
	};

	if (ImGui::SliderInt("Octaves", &mOctaves, 0, 20))
	{
		RecreateGeometry(mTesselation);
	};

	ImGui::Text("World Matrix");

	ImGui::SliderInt("Render Item", &mSelectedRenderItem, 0, mNumRenderItems - 1);

	if (ImGui::SliderFloat3("Position", mPos, -5.0f, 5.0f, "%.1f"))
	{
		mWMatrixChanged = true;

		// Signal to update object constant buffer
		mRenderItems[0]->NumDirtyFrames += mNumFrameResources;
	}

	if (ImGui::SliderFloat3("Rotation", mRot, -5.0f, 5.0f, "%.1f"))
	{
		
		mWMatrixChanged = true;

		// Signal to update object constant buffer
		mRenderItems[0]->NumDirtyFrames += mNumFrameResources;
	}

	if (ImGui::SliderFloat("Scale", &mScale, 0.0f, 5.0f, "%.1f"))
	{
		mWMatrixChanged = true;

		// Signal to update object constant buffer
		mRenderItems[0]->NumDirtyFrames += mNumFrameResources;
	}

	if (ImGui::Checkbox("VSync", &mGraphics->mVSync));

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

}

void App::CreateIcosohedron()
{
	if (!mIcosohedron) { mIcosohedron.release(); }
	mIcosohedron = std::make_unique<Icosahedron>(mFrequency, mRecursions, mOctaves, mEyePos, mTesselation);
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
	//icoRenderItem->WorldMatrix = mGUIWorldMatrix;
	XMStoreFloat4x4(&icoRenderItem->WorldMatrix, XMMatrixIdentity() * /*XMMatrixScaling(100, 100, 100) **/ XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	icoRenderItem->ObjConstantBufferIndex = 0;
	icoRenderItem->Geometry = mIcosohedron->mGeometryData.get();
	icoRenderItem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	icoRenderItem->IndexCount = mIcosohedron->mGeometryData->mIndices.size();
	icoRenderItem->StartIndexLocation = 0;
	icoRenderItem->BaseVertexLocation = 0;
	mRenderItems.push_back(icoRenderItem);

	mIcoLight = make_unique<Icosahedron>(0, 2, 0, mEyePos, false);
	for (auto& vertex : mIcoLight->mGeometryData->mVertices)
	{
		vertex.Colour = XMFLOAT4{ 1.0f,0.8f,0.0f,1.0f };
	}
	mIcoLight->mGeometryData->CalculateBufferData(mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	//mGraphics->ExecuteCommands();
	//mGraphics->mCommandList.Reset();

	//RenderItem* lightRitem = new RenderItem();
	//XMStoreFloat4x4(&lightRitem->WorldMatrix, XMMatrixIdentity() * XMMatrixScaling(0.05, 0.05, 0.05)* XMMatrixTranslation(0.0f,0.0f,8.0f));
	//lightRitem->ObjConstantBufferIndex = 1;
	//lightRitem->Geometry = mIcoLight->mGeometryData.get();
	//lightRitem->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//lightRitem->IndexCount = mIcoLight->mGeometryData->mIndices.size();
	//lightRitem->StartIndexLocation = 0;
	//lightRitem->BaseVertexLocation = 0;
	//mRenderItems.push_back(lightRitem);

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

void App::CreateCBVHeap()
{
	UINT objCount = (UINT)mNumRenderItems;
	// Need a CBV descriptor for each object for each frame resource,
	UINT numDescriptors = (objCount + 1) * mNumFrameResources; // +1 for the perFrameCB for each frame resource.

	// Save an offset to the start of the per frame CBVs.
	mPassCbvOffset = objCount * mNumFrameResources;

	// Save an offset to the start of the GUI SRVs
	mGUISRVOffset = (objCount * mNumFrameResources) + mNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors + 1; // For GUI
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	if (FAILED(mGraphics->mD3DDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCBVHeap))))
	{
		MessageBox(0, L"Create descriptor heap failed", L"Error", MB_OK);
	}
}

void App::CreateConstantBuffers()
{
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(FrameResource::mPerObjectConstants));
	UINT numObjects = (UINT)mNumRenderItems;

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
	//if(mPSO) mPSO->Release();

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
	if (mWireframe) psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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
	if (FAILED(mGraphics->mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO))))
	{
		MessageBox(0, L"Pipeline State Creation failed", L"Error", MB_OK);
	}
}

void App::RecreateGeometry(bool tesselation)
{
	static bool firstFrame = true;
	if (!firstFrame)
	{
		mIcosohedron->ResetGeometry(mEyePos, mFrequency, mRecursions, mOctaves, tesselation);
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


void App::Update(float frameTime)
{
	UpdateCamera();

	CycleFrameResources();

	// Create geometry if its the first frame
	static bool firstGenerationFrame = true;
	if (firstGenerationFrame)
	{
		RecreateGeometry(mTesselation);
		firstGenerationFrame = false;
	}

	// If tesselation is enabled recreate geometry
	if (mTesselation)
	{
		RecreateGeometry(mTesselation);
	}

	// If the world matrix has changed
	if (mWMatrixChanged)
	{
		// Create identity matrices
		XMMATRIX IDMatrix = XMMatrixIdentity();
		XMMATRIX translationMatrix = XMMatrixIdentity();

		// Apply transformations from GUI
		translationMatrix *= XMMatrixTranslation(mPos[0], mPos[1], mPos[2]);
		
		translationMatrix *= XMMatrixRotationX(mRot[0]) * XMMatrixRotationY(mRot[1]) * XMMatrixRotationZ(mRot[2]);
		
		translationMatrix *= XMMatrixScaling(mScale, mScale, mScale);

		// Set result to icosahedron's world matrix
		XMStoreFloat4x4(&mGUIWorldMatrix, IDMatrix *= translationMatrix);

		// Set first render item to icosahedrons world matrix
		mRenderItems[mSelectedRenderItem]->WorldMatrix = mGUIWorldMatrix;
		mRenderItems[mSelectedRenderItem]->NumDirtyFrames += mNumFrameResources;

		mWMatrixChanged = false;
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
	perFrameConstantBuffer.RenderTargetSize = XMFLOAT2((float)mWidth, (float)mHeight);
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
	auto commandAllocator = mCurrentFrameResource->mCommandAllocator;
	auto commandList = mGraphics->mCommandList;

	// We can only reset when the associated command lists have finished execution on the GPU.
	if (FAILED(commandAllocator->Reset()))
	{
		MessageBox(0, L"Command Allocator reset failed", L"Error", MB_OK);
	}

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	if (FAILED(commandList->Reset(commandAllocator.Get(), mPSO.Get())))
	{
		MessageBox(0, L"Command List reset failed", L"Error", MB_OK);
	}

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	commandList->RSSetViewports(1, &mGraphics->mViewport);
	commandList->RSSetScissorRects(1, &mGraphics->mScissorRect);

	// Clear the back buffer and depth buffer.
	commandList->ClearRenderTargetView(mGraphics->MSAAView(), mGraphics->mBackgroundColour, 0, nullptr);
	commandList->ClearDepthStencilView(mGraphics->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Select MSAA texture as render target
	commandList->OMSetRenderTargets(1, &mGraphics->MSAAView(), true, &mGraphics->DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCBVHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mGraphics->mCbvSrvUavDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(commandList.Get());

	mGraphics->ResolveMSAAToBackBuffer(commandList.Get());

	RenderGUI();

	// Done recording commands so close command list
	if (FAILED(mGraphics->mCommandList->Close()))
	{
		MessageBox(0, L"Command List close failed", L"Error", MB_OK);
	}

	// Add the command list to the queue for execution
	ID3D12CommandList* cmdLists[] = { commandList.Get() };
	mGraphics->mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Swap the back and front buffers
	if (FAILED(mGraphics->mSwapChain->Present(mGraphics->mVSync, 0)))
	{
		MessageBox(0, L"Swap chain present failed", L"Error", MB_OK);
	}
	mGraphics->mCurrentBackBuffer = (mGraphics->mCurrentBackBuffer + 1) % mGraphics->mSwapChainBufferCount;

	// Advance fence value
	mCurrentFrameResource->Fence = ++mGraphics->mCurrentFence;

	// Tell command queue to set new fence point, will only be set when the GPU gets to new fence value.
	mGraphics->mCommandQueue->Signal(mGraphics->mFence.Get(), mGraphics->mCurrentFence);
}

void App::RenderGUI()
{
	ImGui::Render();

	auto commandList = mGraphics->mCommandList;

	// Transition back buffer to RT
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGraphics->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Get handle to the depth stencle and offset
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapView(mGraphics->mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	dsvHeapView.Offset(1, mGraphics->mDsvDescriptorSize);

	// Select Back buffer as render target
	commandList->ClearDepthStencilView(dsvHeapView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	commandList->OMSetRenderTargets(1, &mGraphics->CurrentBackBufferView(), true, &dsvHeapView);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

	// Transition back buffer to present
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGraphics->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
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
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
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
		mRadius = std::clamp(mRadius, 0.1f, 15.0f);
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
		else if (event.button.button == 2) { mWireframe = !mWireframe; CreatePSO(); }
	}
	else if (event.type == SDL_MOUSEBUTTONUP)
	{
		if (event.button.button == 3) { mRightMouse = false; }
		else if (event.button.button == 1) { mLeftMouse = false; }
	}
}

void App::Resized()
{
	// The window resized, so update the aspect ratio and recompute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(mWidth) / mHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProjectionMatrix, proj);
}

App::~App()
{
	// Empty the command queue
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }

	//delete mRenderItems[1]->Geometry;

	// Delete render items
	for (auto& renderItem : mRenderItems)
	{
		delete renderItem;
	}

	// Shutdown ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// Shutdown SDL
	SDL_DestroyWindow(mWindow);
	SDL_Quit();
}