#pragma once

#include "SDL.h"
#include <DirectXMath.h>
#include "Window.h"
#include <algorithm>
#include "Utility.h"

using namespace DirectX;

class Camera
{
public:
	Camera(Window* mWindow);
	~Camera();

	void MouseMoved(SDL_Event& event, Window* mWindow);

	void Update();

	void WindowResized(Window* window);

	POINT mLastMousePos = {0,0};

	XMFLOAT3 mPos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mLastPos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mViewMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mProjectionMatrix = MakeIdentity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;
};

