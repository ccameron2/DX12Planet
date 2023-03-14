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

	if (ImGui::SliderInt("LOD", &mLOD, 0, 10))
	{
		mPlanetUpdated = true;
	};

	//if (ImGui::Checkbox("Tesselation", &mTesselation))
	//{
	//	mUpdated = true;
	//};

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

	if (ImGui::SliderInt("Model", &mSelectedModel, 0, numModels - 1))
	{

	};

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

	if (ImGui::SliderFloat3("Light Direction", mLightDir, -1, +1));

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
	//Window event occured
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplSDL2_ProcessEvent(&event);
	if (io.WantCaptureMouse || io.WantCaptureKeyboard)
	{
		return true;
	}
}
