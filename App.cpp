#include "App.h"

// Construct and set reference to self for WndProc
App::App()
{
	Initialize();
	Run();
}

bool App::InitWindow()
{
	return true;
}

// Calculate stats for rendering frame
void App::FrameStats()
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

		mWindow.setTitle(windowText);
		// Reset
		frameCount = 0;
		timeElapsed += 1.0f;
	}
}

// Run the app
void App::Run()
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

	while (mWindow.isOpen())
	{
		PollEvents();
		float frameTime = mTimer.GetLapTime();
		FrameStats();
		Update(frameTime);
		mGraphics->Draw(frameTime, geometryData);
	}	
}

void App::Initialize()
{
	mWindow.create(sf::VideoMode(mWidth, mHeight), "D3D12 Masters Project", sf::Style::Default);

	mGraphics = make_unique<Graphics>(mWindow.getSystemHandle(), mWidth, mHeight);

	CreateIcosohedron();

	mGraphics->ExecuteCommands();
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

void App::CreateIcosohedron()
{
	mIcosohedron = std::make_unique<Icosahedron>(8, 36, mGraphics->mD3DDevice.Get(), mGraphics->mCommandList.Get());
}

void App::MouseMoved(sf::Event event)
{
	int mouseX, mouseY;
	mouseX = event.mouseMove.x;
	mouseY = event.mouseMove.y;
	if (mLeftMouse)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(mouseX - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(mouseY - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = std::clamp(mPhi, 0.1f, XM_PI - 0.1f);
	}
	else if (mRightMouse)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(mouseX - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(mouseY - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = std::clamp(mRadius, 3.0f, 15.0f);
	}
	mLastMousePos.x = mouseX;
	mLastMousePos.y = mouseY;
}

void App::PollEvents()
{
	sf::Event event;
	while (mWindow.pollEvent(event))
	{
		sf::Vector2u size;
		switch (event.type)
		{
		case sf::Event::Closed:
			mWindow.close();
			break;
		case sf::Event::LostFocus:
			mAppPaused = true;
			mTimer.Stop();
			break;
		case sf::Event::GainedFocus:
			mAppPaused = false;
			mTimer.Start();
			break;
		case sf::Event::Resized:
			size = mWindow.getSize();
			mWidth = size.x; mHeight = size.y;
			mGraphics->Resize(mWidth, mHeight);
			mTimer.Start();
			break;
		case sf::Event::MouseButtonPressed:
			if (event.mouseButton.button == sf::Mouse::Right) { mRightMouse = true; }
			else if (event.mouseButton.button == sf::Mouse::Left) { mLeftMouse = true; }
			break;
		case sf::Event::MouseButtonReleased:
			if (event.mouseButton.button == sf::Mouse::Right) { mRightMouse = false; }
			else if (event.mouseButton.button == sf::Mouse::Left) { mLeftMouse = false; }
			break;
		case sf::Event::MouseMoved:
			MouseMoved(event);
			break;
		}
	}
}

App::~App()
{
	if (mGraphics->mD3DDevice != nullptr) { mGraphics->EmptyCommandQueue(); }
}