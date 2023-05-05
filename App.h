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

	~App();
private:
	void Run();

	void Initialize();

	Timer mTimer;

	void Update(float frameTime);
	void Draw(float frameTime);

	void RecreatePlanetGeometry();

	void UpdatePlanetBuffers();

	void ProcessEvents(SDL_Event& e);

	void CreateMaterials();

	void CreateSkybox();

	// List of materials
	vector<Material*> mMaterials;
	
	// Sky and water models
	Model* mSkyModel;
	Model* mWaterModel;

	// Sky material
	Material* mSkyMat;

	// Graphics object
	unique_ptr<Graphics> mGraphics;
	
	// Window object
	unique_ptr<Window> mWindow;

	// Planet object and model
	unique_ptr<Planet> mPlanet;
	Model* mPlanetModel;

	// Lists of models by PSO
	vector<Model*> mModels;
	vector<Model*> mTexModels;
	vector<Model*> mSimpleTexModels;
	vector<Model*> mColourModels;
	int mNumModels = 0;

	// Camera object
	unique_ptr<Camera> mCamera;

	// GUI object
	unique_ptr<GUI> mGUI;

	// Light values
	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	// Wireframe mode
	bool mWireframe = false;

	// Current material index
	int mCurrentMatCBIndex = 0;

	void LoadModels();
	void UpdateSelectedModel();
	void UpdatePerObjectConstantBuffers();
	void UpdatePerFrameConstantBuffer();
	void UpdatePerMaterialConstantBuffers();

	void BuildFrameResources();

	void DrawPlanet(ID3D12GraphicsCommandList* commandList);
	void DrawModels(ID3D12GraphicsCommandList* commandList);
	void StartFrame();
	void EndFrame();

	void RenderThread(int thread);
	void RenderChunks(int thread, int start, int end);

	struct WorkerThread
	{
		std::thread             thread;
		std::condition_variable workReady;
		std::mutex              lock;
	};

	struct RenderWork
	{
		bool complete = true;
		int  start = 0;
		int  end = 0;
	};

	static const int MAX_WORKERS = 128;
	std::pair<WorkerThread, RenderWork> mRenderWorkers[MAX_WORKERS];
	int mNumRenderWorkers = 0;
};