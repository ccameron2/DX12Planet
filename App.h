#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#define SDL_MAIN_HANDLED
#define SDL_ENABLE_SYSWM_WINDOWS

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "d3dx12.h"
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>

#include "Planet.h"
#include "Model.h"

#include <SDL.h>
#include <imgui.h>
#include "GUI.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_dx12.h"

#include "Window.h"
#include "Camera.h"

#include "Timer.h"
#include "Utility.h"
#include "Icosahedron.h"
#include "Graphics.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "SRVDescriptorHeap.h"
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>

#include <fstream>

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class App
{
public:
	App();

	// Flush command queue to prevent GPU crash on exit
	~App();

	void Run();

	void Initialize();

	void CreateTextures();

private:
	Timer mTimer;

	void Update(float frameTime);
	void CycleFrameResources();
	void Draw(float frameTime);
	
	void CreateRootSignature();
	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	void CreateShaders();
	void CreatePSO();
	void RecreatePlanetGeometry();
	void ProcessEvents(SDL_Event& e);
	void CreateMaterials();

	vector<Texture*> mTextures;
	vector<Material*> mMaterials;

	unique_ptr<Graphics> mGraphics;
	unique_ptr<Window> mWindow;
	SDL_Surface mScreenSurface;

	unique_ptr<Planet> mPlanet;

	// If diffent PSOs needed then use different lists
	vector<Model*> mModels;
	vector<Model*> mTexModels;
	vector<Model*> mColourModels;

	XMFLOAT4X4 mGUIWorldMatrix = MakeIdentity4x4();

	unique_ptr<Camera> mCamera;
	unique_ptr<GUI> mGUI;

	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	int mNumModels = 0;

	bool mWireframe = false;
	const static int mNumFrameResources = 3;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	unique_ptr<SRVDescriptorHeap> mSRVDescriptorHeap;

	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;

	// Compiled shader variables
	ComPtr<ID3DBlob> mColourVSByteCode = nullptr;
	ComPtr<ID3DBlob> mColourPSByteCode = nullptr;
	ComPtr<ID3DBlob> mTexVSByteCode = nullptr;
	//ComPtr<ID3DBlob> mTexPSByteCode = nullptr;

	ComPtr<ID3DBlob> mTexPSByteCode = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature;

	// Input layout
	std::vector<D3D12_INPUT_ELEMENT_DESC> mColourInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTexInputLayout;

	ComPtr<ID3D12PipelineState> mSolidPSO = nullptr;
	ComPtr<ID3D12PipelineState> mWireframePSO = nullptr;
	ComPtr<ID3D12PipelineState> mTexPSO = nullptr;

	ComPtr<ID3D12PipelineState> mPlanetPSO = nullptr;

	ID3D12PipelineState* mCurrentPSO = nullptr;

	ComPtr<ID3DBlob> mPlanetVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPlanetPSByteCode = nullptr;


	void UpdatePerObjectConstantBuffers();
	void UpdatePerFrameConstantBuffer();
	void UpdatePerMaterialConstantBuffers();

	void BuildFrameResources();

	void DrawPlanet(ID3D12GraphicsCommandList* commandList);
	void LoadModels();
	void DrawModels(ID3D12GraphicsCommandList* commandList);
	void StartFrame();
	void EndFrame();
};