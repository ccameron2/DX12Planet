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

//#include "DDSTextureLoader.h"

#include <SDL.h>
#include <imgui.h>

#include "imgui_impl_sdl.h"
#include "imgui_impl_dx12.h"

#include "Timer.h"
#include "Utility.h"
#include "Icosahedron.h"
#include "Graphics.h"
#include "UploadBuffer.h"
#include "FrameResource.h"

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

	bool InitWindow();
	void SetupGUI();
	void ShowGUI();
	void FrameStats();
	void Update(float frameTime);
	void Draw(float frameTime);
	
	void CreateIcosohedron();
	void MouseMoved(SDL_Event&);
	void PollEvents(SDL_Event& e);
	void Resized();
	void UpdateCamera();
	void RenderGUI();
	unique_ptr<Graphics> mGraphics;
	SDL_Window* mWindow;
	SDL_Surface mScreenSurface;

	struct RenderItem
	{
		XMFLOAT4X4 WorldMatrix = MakeIdentity4x4();
		int NumDirtyFrames = mNumFrameResources;
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
	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mViewMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mProjectionMatrix = MakeIdentity4x4();

	bool mAppPaused = false;
	bool mResizing = false;
	bool mFullscreen = false;
	bool mRightMouse = false;
	bool mLeftMouse = false;
	bool mMouseFocus = false;
	bool mKeyboardFocus = false;
	bool mMinimized = false;
	bool mQuit = false;
	POINT mLastMousePos;

	int mWidth = 800;
	int mHeight = 600;

	std::unique_ptr<Icosahedron> mIcosohedron;
	float frequency = 0.5f;
	int recursions = 6;
	int octaves = 8;


	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	std::string mMainCaption = "D3D12 Engine Masters";

	const static int mNumFrameResources = 3;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	ComPtr<ID3D12DescriptorHeap> mGUIHeap;
	ComPtr<ID3D12DescriptorHeap> mCBVHeap;
	UINT mPassCbvOffset = 0;
	void BuildFrameResources();
	void UpdatePerObjectConstantBuffers();
	void UpdatePerFrameConstantBuffers();
	void CreateConstantBuffers();
	void CreateRenderItems();
	void CreateCBVHeap();
	void DrawRenderItems(ID3D12GraphicsCommandList* commandList);
};

struct Cube
{
	vector<Vertex> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};
	vector<uint32_t> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};
};