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
#include <SFML/Window.hpp>

#include "Timer.h"
#include "Utility.h"
#include "Icosahedron.h"
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
	void Update(float frameTime);
	
	void MouseDown(WPARAM buttonState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mHWND); }
	void MouseUp(WPARAM buttonState, int x, int y) { ReleaseCapture(); }
	void MouseMove(WPARAM buttonState, int x, int y);

	void CreateIcosohedron();

private:
	static App* mApp;
	unique_ptr<Graphics> mGraphics;
	sf::Window mWindow;
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

	std::unique_ptr<Icosahedron> mIcosohedron;

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	D3D_DRIVER_TYPE mD3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	std::wstring mMainCaption = L"D3D12";
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
	vector<uint16_t> indices =
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