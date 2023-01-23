#include "GUI.h"

GUI::GUI()
{
}

GUI::~GUI()
{
	// Shutdown ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void GUI::SetupGUI(ID3D12DescriptorHeap* cbvHeap, UINT guiSrvOffset, UINT cbvDescriptorSize, SDL_Window* window, ID3D12Device* device, UINT numFrameResources, DXGI_FORMAT backBufferFormat)
{
	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	auto cpuhandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvHeap->GetCPUDescriptorHandleForHeapStart());
	cpuhandle.Offset(guiSrvOffset, cbvDescriptorSize);

	auto gpuhandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvHeap->GetGPUDescriptorHandleForHeapStart());
	gpuhandle.Offset(guiSrvOffset, cbvDescriptorSize);

	ImGui_ImplSDL2_InitForD3D(window);
	ImGui_ImplDX12_Init(device, numFrameResources, backBufferFormat, cbvHeap, cpuhandle, gpuhandle);
}

void GUI::NewFrame()
{
	// Start the ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void GUI::Update(int numRenderItems)
{
	//static bool showDemoWindow = false;
	//ImGui::ShowDemoWindow(&showDemoWindow);

	ImGui::Begin("Planet");

	ImGui::Text("Geometry");

	if (ImGui::SliderInt("Recursions", &mRecursions, 0, 10))
	{
		mUpdated = true;
	};

	if (ImGui::Checkbox("Tesselation", &mTesselation))
	{
		mUpdated = true;
	};

	ImGui::Text("Noise");

	if (ImGui::SliderFloat("Noise Frequency", &mFrequency, 0.0f, 1.0f, "%.1f"))
	{
		mUpdated = true;
	};

	if (ImGui::SliderInt("Octaves", &mOctaves, 0, 20))
	{
		mUpdated = true;
	};

	ImGui::Text("World Matrix");

	if (ImGui::SliderInt("Render Item", &mSelectedRenderItem, 0, numRenderItems - 1))
	{
		mPos[0] = 0; mPos[1] = 0; mPos[2] = 0;
		mRot[0] = 0; mRot[1] = 0; mRot[2] = 0;
		mScale = 1;
		mWMatrixChanged = true;
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

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

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
