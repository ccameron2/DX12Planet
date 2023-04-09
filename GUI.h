#pragma once

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_sdl.h"
#include "SDL.h"
#include "d3dx12.h"
#include "SRVDescriptorHeap.h"
#include "Model.h"

class GUI
{
	UINT mGuiSrvOffset = 0;
public:
	GUI(SRVDescriptorHeap* descriptorHeap, SDL_Window* window, ID3D12Device* device, UINT numFrameResources, DXGI_FORMAT backBufferFormat);
	~GUI();
	void NewFrame();
	void Update(int numRenderItems);
	void UpdateModelData(Model* model);
	void Render(ID3D12GraphicsCommandList* commandList, ID3D12Resource* currentBackBuffer, D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView, ID3D12DescriptorHeap* dsvHeap, UINT dsvDescriptorSize);
	bool ProcessEvents(SDL_Event& event);
	bool mPlanetUpdated = false;
	float mFrequency = 0.5f;
	int mLOD = 4;
	int mMaxLOD = 6;
	int mOctaves = 8;
	float mPos[3] = {0,0,0};
	float mRot[3] = {0,0,0};
	float mScale = 1;
	bool mCameraOrbit = true;

	XMFLOAT3 mInPosition{0,0,0};
	XMFLOAT3 mInRotation{0,0,0};
	XMFLOAT3 mInScale{0,0,0};

	bool mWMatrixChanged = false;
	bool mTesselation = false;
	int mSelectedModel = 0;
	bool mVSync = false;
	float mLightDir[3] = { -0.577f, -0.577f, 0.577f };
};

