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
#include "Common.h"

#include <thread>
#include <condition_variable>

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

	void Update(float frameTime);
	void Draw(float frameTime);

	void RecreatePlanetGeometry();
	void UpdatePlanetBuffers();
	void ProcessEvents(SDL_Event& e);
	void CreateMaterials();
	void CreateSkybox();

	vector<Texture*> mTextures;
	vector<Material*> mMaterials;
	Model* mSkyModel;
	Model* mWaterModel;

	unique_ptr<Graphics> mGraphics;
	unique_ptr<Window> mWindow;
	SDL_Surface mScreenSurface;

	unique_ptr<Planet> mPlanet;

	// If diffent PSOs needed then use different lists
	vector<Model*> mModels;
	vector<Model*> mTexModels;
	vector<Model*> mSimpleTexModels;
	vector<Model*> mColourModels;

	unique_ptr<Camera> mCamera;
	unique_ptr<GUI> mGUI;

	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	int mNumModels = 0;

	bool mWireframe = false;

	int mCurrentMatCBIndex = 0;

	Material* mSkyMat;

	ID3D12PipelineState* mCurrentPSO = nullptr;

	void UpdateSelectedModel();
	void UpdatePerObjectConstantBuffers();
	void UpdatePerFrameConstantBuffer();
	void UpdatePerMaterialConstantBuffers();

	void BuildFrameResources();

	void DrawPlanet(ID3D12GraphicsCommandList* commandList);
	void LoadModels();
	void DrawModels(ID3D12GraphicsCommandList* commandList);
	void StartFrame();
	void EndFrame();

	// A worker thread wakes up when work is signalled to be ready, and signals back when the work is complete.
	// Same condition variable is used for signalling in both directions. A mutex is used to guard data shared between threads
	struct WorkerThread
	{
		std::thread             thread;
		std::condition_variable workReady;
		std::mutex              lock;
	};

	// Data describing work to do by a worker thread - render a lot of boats here
	struct RenderWork
	{
		bool complete = true;
		int  start = 0;
		int  end = 0;
	};

	// A pool of worker threads, each with its associated work
	// A more flexible system could generalise the type of work that worker threads can do
	static const int MAX_WORKERS = 128;
	std::pair<WorkerThread, RenderWork> mRenderWorkers[MAX_WORKERS];
	int mNumRenderWorkers = 0;

	ID3D12GraphicsCommandList* mCurrentMainCommandList;
};