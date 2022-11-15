#include "App.h"
#include "WindowsX.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd as messages can be received before hwnd is valid
	return App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

// Reference to pass to WndProc to take control of msg procedures
App* App::mApp = nullptr;
App* App::GetApp() { return mApp;}

// Construct and set reference to self for WndProc
App::App(HINSTANCE hInstance) : mHInstance(hInstance)
{
    mApp = this;
	Initialize();
	Run();
}

bool App::InitWindow()
{
	mWindow.create(sf::VideoMode(mWidth, mHeight), "D3D12 Masters Project", sf::Style::Default);
	//// Register windows class
	//WNDCLASS wc;
	//wc.style = CS_HREDRAW | CS_VREDRAW;
	//wc.lpfnWndProc = App::WndProc;
	//wc.cbClsExtra = 0;
	//wc.cbWndExtra = 0;
	//wc.hInstance = mHInstance;
	//wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	//wc.hCursor = LoadCursor(0, IDC_ARROW);
	//wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	//wc.lpszMenuName = 0;
	//wc.lpszClassName = L"Wnd";
	//if (!RegisterClass(&wc)) { MessageBox(0, L"Register Class Failed.", L"Error", MB_OK); return false; }

	//// Calc window dimensions
	//RECT rect = { 0, 0, mWidth, mHeight };
	//AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	//int width = rect.right - rect.left;
	//int height = rect.bottom - rect.top;

	//// Create window and handle
	//mHWND = CreateWindow(L"Wnd", L"D3D12", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mHInstance, 0);
	//if (!mHWND)
	//{
	//	MessageBox(0, L"Create Window Failed.", L"Error", MB_OK);
	//	return false;
	//}

	//ShowWindow(mHWND, SW_SHOW);
	//UpdateWindow(mHWND);

	return true;
}

// Calculate stats for rendering frame
void App::CalcFrameStats()
{
	static int frameCount = 0;
	static float timeElapsed = 0.0f;

	frameCount++;

	// Calculate averages
	if ((mTimer.GetTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCount;
		float ms = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring msStr = std::to_wstring(ms);

		std::wstring windowText = mMainCaption + L"    fps: " + fpsStr + L"   ms: " + msStr;

		SetWindowText(mHWND, windowText.c_str());

		// Reset
		frameCount = 0;
		timeElapsed += 1.0f;
	}
}


// Run the app
int App::Run()
{
	MSG msg = {};

	// Start timer for frametime
	mTimer.Start();

	// Create test cube
	unique_ptr<GeometryData> geometry = make_unique<GeometryData>();
	Cube cube;
	geometry->mVertices = cube.vertices;
	geometry->mIndices = cube.indices;
	mGraphics->mCommandList->Reset(mGraphics->mCommandAllocator.Get(), nullptr);
	geometry->CalculateBufferData(mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
	mGraphics->ExecuteCommands();
	////////////

	// Collect all geometry into vector
	vector<GeometryData*> geometryData = { mIcosohedron->mGeometryData.get(),geometry.get()};

	sf::Event event;
	while (mWindow.isOpen())
	{
		float frameTime = mTimer.GetLapTime();
		while (mWindow.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				mWindow.close();
				break;
			default:
				break;
			}
		}
		CalcFrameStats();
		Update(frameTime);
		mGraphics->Draw(frameTime, geometryData);
	}
	
	
	return 0;
	//// While window is open
	//while (msg.message != WM_QUIT)
	//{
	//	// Process windows messages
	//	if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	//	{
	//		TranslateMessage(&msg);
	//		DispatchMessage(&msg);
	//	}
	//	else // No messages left
	//	{
	//		// Get frametime from timer
	//		float frameTime = mTimer.GetLapTime();

	//		// If app isnt paused
	//		if (!mAppPaused)
	//		{
	//			CalcFrameStats();
	//			Update(frameTime);
	//			mGraphics->Draw(frameTime, geometryData);
	//		}
	//		else { Sleep(100); }
	//	}
	//}

	//return (int)msg.wParam;
}

bool App::Initialize()
{
	if (!InitWindow()) { return false; }
	mGraphics = make_unique<Graphics>(mWindow.getSystemHandle(), mWidth, mHeight);

	CreateIcosohedron();

	mGraphics->ExecuteCommands();

	return true;
}

void App::Update(float frameTime)
{
	// Convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mGraphics->mViewMatrix, view);

	XMMATRIX world = XMLoadFloat4x4(&mGraphics->mWorldMatrix);

	XMMATRIX proj = XMLoadFloat4x4(&mGraphics->mProjectionMatrix);
	XMMATRIX worldViewProj = world * view * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	Graphics::mPerObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mGraphics->mPerObjectConstantBuffer->Copy(0,objConstants);
}


void App::MouseMove(WPARAM buttonState, int x, int y)
{
	if ((buttonState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = std::clamp(mPhi, 0.1f, PI - 0.1f);
	}
	else if ((buttonState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = std::clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void App::CreateIcosohedron()
{
	mIcosohedron = std::make_unique<Icosahedron>(8, 36, mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
}

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// Pause app when window becomes inactive
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WHen user changes window size
	case WM_SIZE:
		// Save new dimensions.
		mWidth = LOWORD(lParam);
		mHeight = HIWORD(lParam);
		if (mGraphics)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimised = true;
				mMaximised = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimised = false;
				mMaximised = true;
				mGraphics->Resize(mWidth,mHeight);
			}
			else if (wParam == SIZE_RESTORED)
			{
				// Restoring from minimized
				if (mMinimised)
				{
					mAppPaused = false;
					mMinimised = false;
					mGraphics->Resize(mWidth, mHeight);
				}

				// Restoring from maximized
				else if (mMaximised)
				{
					mAppPaused = false;
					mMaximised = false;
					mGraphics->Resize(mWidth, mHeight);
				}
				else if (mResizing)
				{
					// Only resize when user has finished with drag bars
				}
				else
				{
					mGraphics->Resize(mWidth, mHeight);
				}
			}
		}
		return 0;

		// Start resize
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// End resize and reset everything to new dimensions
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		mGraphics->Resize(mWidth, mHeight);
		return 0;

		// When window is destroyed
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// When menu is active and an invalid key is pressed
	case WM_MENUCHAR:
		// Remove alt-enter beep
		return MAKELRESULT(0, MNC_CLOSE);

		// Prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

		// Mouse button down events
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		MouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Mouse button up events
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		MouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Mouse move event
	case WM_MOUSEMOVE:
		MouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
		
		// Key up events
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

App::~App()
{
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }
}