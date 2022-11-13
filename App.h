#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

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

#include "Timer.h"
#include "Utility.h"
#include "Icosahedron.h"
#include "Common.h"
#include "Graphics.h"
#include "UploadBuffer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class App
{
public:
	App(HINSTANCE hInstance);

	// Flush command queue to prevent GPU crash on exit
	~App();

	static App* GetApp();

	int Run();

	bool Initialize();

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); // Windows procedure function for the main app window

private:
	Timer mTimer;

	bool InitWindow();
	void CalcFrameStats();
	void CreateDescriptorHeaps();
	void Update(float frameTime);
	void Draw(float frameTime);
	
	void OnMouseDown(WPARAM buttonState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mHWND); }
	void OnMouseUp(WPARAM buttonState, int x, int y) { ReleaseCapture(); }
	void OnMouseMove(WPARAM buttonState, int x, int y);

	ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	void CreateConstantBuffers();
	void CreateRootSignature();
	void CreateShaders();
	void CreateIcosohedron();
	void CreatePSO();

private:
	static App* mApp;
	unique_ptr<Graphics> mGraphics;

	HINSTANCE mHInstance = nullptr; // App instance handle
	HWND mHWND = nullptr; // Window handle

	bool mAppPaused = false;
	bool mMinimised = false;
	bool mMaximised = false;
	bool mResizing = false;
	bool mFullscreen = false;

	POINT mLastMousePos;
	float PI = 3.14159;

	int mWidth = 800;
	int mHeight = 600;

	//bool m4xMSAA = false;
	//bool m4xMSAAQuality = 0; // Quality level of MSAA
	ComPtr<ID3D12DescriptorHeap> mCBVHeap;

	struct mPerObjectConstants
	{
		XMFLOAT4X4 WorldViewProj;
	};

	// Per Object constant buffer
	unique_ptr <UploadBuffer<mPerObjectConstants>> mPerObjectConstantBuffer;

	ComPtr<ID3D12RootSignature> mRootSignature;

	// Shader variables
	ComPtr<ID3DBlob> mVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPSByteCode = nullptr;

	// Input layout
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<Icosahedron> mIcosohedron;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	std::wstring mMainCaption = L"D3D12";
};

