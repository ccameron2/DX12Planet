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

	// Input data
	float mFrequency = 0.5f;
	int mLOD = 2;
	int mSeed = rand();
	int mMaxLOD = 6;
	int mOctaves = 8;
	float mPos[3] = {0,0,0};
	float mRot[3] = {0,0,0};
	float mScale = 1;
	float mSpeedMultipler = 1;
	bool mCLOD = false;
	int mDebugTex = 0.f;
	bool mCameraOrbit = true;
	bool mInvertY = true;
	bool mVSync = false;
	float mLightDir[3] = { -0.577f, -0.577f, 0.577f };

	XMFLOAT3 mInPosition{0,0,0};
	XMFLOAT3 mInRotation{0,0,0};
	XMFLOAT3 mInScale{0,0,0};

	// Report data
	bool mWMatrixChanged = false;
	int mSelectedModel = 1;
	bool mPlanetUpdated = false;

};

