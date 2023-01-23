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
//#include "DDSTextureLoader.h"

#include <SDL.h>
#include <imgui.h>
#include "GUI.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_dx12.h"

#include "Window.h"

#include "Timer.h"
#include "Utility.h"
#include "Icosahedron.h"
#include "Graphics.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "DescriptorHeap.h"

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

private:
	Timer mTimer;

	void SetupGUI();
	void Update(float frameTime);
	void CycleFrameResources();
	void Draw(float frameTime);
	
	void CreateIcosohedron();
	void CreateRootSignature();
	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	void CreateShaders();
	void CreatePSO();
	void RecreateGeometry(bool tesselation);
	void MouseMoved(SDL_Event&);
	void ProcessEvents(SDL_Event& e);
	void Resized();
	void UpdateCamera();
	unique_ptr<Graphics> mGraphics;
	unique_ptr<Window> mWindow;
	SDL_Surface mScreenSurface;

	unique_ptr<Planet> mPlanet;

	unique_ptr<GeometryData> mSkullGeometry;
	unique_ptr<Icosahedron> mIcoLight;
	struct RenderItem
	{
		XMFLOAT4X4 WorldMatrix = MakeIdentity4x4();
		int NumDirtyFrames = 3;
		UINT ObjConstantBufferIndex = -1;
		GeometryData* Geometry = nullptr;
		D3D12_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		int BaseVertexLocation = 0;
	};
	// If diffent PSOs needed then use different lists
	vector<RenderItem*> mRenderItems;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mLastEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mViewMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mProjectionMatrix = MakeIdentity4x4();
	
	XMFLOAT4X4 mGUIWorldMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mIcoTranslationMatrix = MakeIdentity4x4();

	unique_ptr<GUI> mGUI;

	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	POINT mLastMousePos;

	std::unique_ptr<Icosahedron> mIcosohedron;

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	bool mWireframe = false;
	int mNumRenderItems = 0;
	const static int mNumFrameResources = 3;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	unique_ptr<DescriptorHeap> mCBVDescriptorHeap;

	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;

	// Compiled shader variables
	ComPtr<ID3DBlob> mVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPSByteCode = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature;

	// Input layout
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mSolidPSO = nullptr;
	ComPtr<ID3D12PipelineState> mWireframePSO = nullptr;
	ID3D12PipelineState* mCurrentPSO = nullptr;

	void UpdatePerObjectConstantBuffers();
	void UpdatePerFrameConstantBuffer();

	void CreateRenderItems();

	void BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* commandList);
	void BuildSkullGeometry();
	void StartFrame();
	void EndFrame();
};