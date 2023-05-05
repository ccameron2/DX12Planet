#include "GUI.h"

GUI::GUI(SRVDescriptorHeap* descriptorHeap, SDL_Window* window, ID3D12Device* device, UINT numFrameResources, DXGI_FORMAT backBufferFormat)
{
	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	auto cpuhandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
	cpuhandle.Offset(descriptorHeap->mGuiSrvOffset, descriptorHeap->mDescriptorSize);

	auto gpuhandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
	gpuhandle.Offset(descriptorHeap->mGuiSrvOffset, descriptorHeap->mDescriptorSize);

	ImGui_ImplSDL2_InitForD3D(window);
	ImGui_ImplDX12_Init(device, numFrameResources, backBufferFormat, descriptorHeap->mHeap.Get(), cpuhandle, gpuhandle);
}

GUI::~GUI()
{
	// Shutdown ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void GUI::NewFrame()
{
	// Start the ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void GUI::Update(int numModels)
{
	//static bool showDemoWindow = false;
	//ImGui::ShowDemoWindow(&showDemoWindow);

	ImGui::Begin("Planet");

	ImGui::Text("Geometry");

	if (ImGui::InputInt("Seed", &mSeed, 1, 10))
	{
		mPlanetUpdated = true;
	}

	if (ImGui::Checkbox("CLOD", &mCLOD))
	{
		mPlanetUpdated = true;

	}

	if (mCLOD) mMaxLOD = 6;
	else mMaxLOD = 4;

	if (mLOD > mMaxLOD) mLOD = mMaxLOD;

	if (ImGui::SliderInt("LOD", &mLOD, 0, mMaxLOD))
	{
		mPlanetUpdated = true;
	};

	ImGui::Text("Noise");

	if (ImGui::SliderFloat("Noise Freq", &mFrequency, 0.0f, 1.0f, "%.1f"))
	{
		mPlanetUpdated = true;
	};

	if (ImGui::SliderInt("Octaves", &mOctaves, 0, 20))
	{
		mPlanetUpdated = true;
	};

	ImGui::Text("World Matrix");

	if (ImGui::SliderInt("Model", &mSelectedModel, 1, numModels - 1))
	{

	};

	if (ImGui::SliderInt("TexDebug", &mDebugTex, 0, 7));
	string str = "";
	
	
	if (mDebugTex == 0) str = "PBR";
	else if (mDebugTex == 1) str = "Albedo";
	else if (mDebugTex == 2) str = "Roughness";
	else if (mDebugTex == 3) str = "Normal";
	else if (mDebugTex == 4) str = "Metalness";
	else if (mDebugTex == 5) str = "Height";
	else if (mDebugTex == 6) str = "AO";
	else if (mDebugTex == 7) str = "Emissive";
	else str = "error";

	ImGui::Text(str.c_str());
	
	if (ImGui::InputFloat3("Position", mPos, "%.1f"))
	{
		mWMatrixChanged = true;
	}

	if (ImGui::InputFloat3("Rotation", mRot, "%.1f"))
	{
		mWMatrixChanged = true;
	}

	if (ImGui::InputFloat("Scale", &mScale, 0.0f, 5.0f, "%.1f"))
	{
		mWMatrixChanged = true;
	}

	if (ImGui::Checkbox("VSync", &mVSync));

	if (ImGui::Checkbox("Orbit Camera", &mCameraOrbit));
	if(!mCameraOrbit) if (ImGui::Checkbox("Invert Y", &mInvertY));

	if (ImGui::InputFloat("Speed Mul", &mSpeedMultipler, 1.0f, 10.0f, "%.1f"))
	{
		if (mSpeedMultipler <= 0)
		{
			mSpeedMultipler = 0.1f;
		}
		mWMatrixChanged = true;
	}

	if (ImGui::SliderFloat3("Light Dir", mLightDir, -1, +1));

	ImGui::Text("Average: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	mInPosition.x = mPos[0];
	mInPosition.y = mPos[1];
	mInPosition.z = mPos[2];

	mInRotation.x = mRot[0];
	mInRotation.y = mRot[1];
	mInRotation.z = mRot[2];

	mInScale.x = mScale;
	mInScale.y = mScale;
	mInScale.z = mScale;

	ImGui::End();

}

void GUI::UpdateModelData(Model* model)
{
	mPos[0] = model->mPosition.x;
	mPos[1] = model->mPosition.y;
	mPos[2] = model->mPosition.z;

	mRot[0] = model->mRotation.x;
	mRot[1] = model->mRotation.y;
	mRot[2] = model->mRotation.z;

	mScale = model->mScale.x;
}

void GUI::Render(ID3D12GraphicsCommandList* commandList, ID3D12Resource* currentBackBuffer, D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView, ID3D12DescriptorHeap* dsvHeap, UINT dsvDescriptorSize)
{
	ImGui::Render();

	// Transition back buffer to RT
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Get handle to the depth stencil and offset
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapView(dsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsvHeapView.Offset(1, dsvDescriptorSize);

	// Select Back buffer as render target
	commandList->ClearDepthStencilView(dsvHeapView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &dsvHeapView);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

	// Transition back buffer to present
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
}

bool GUI::ProcessEvents(SDL_Event& event)
{
	// Window event occured
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplSDL2_ProcessEvent(&event);
	if (io.WantCaptureMouse || io.WantCaptureKeyboard)
	{
		return true;
	}
}
