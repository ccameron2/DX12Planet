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

	void Update(float frameTime = 0, bool orbit = true, bool invertMouse = false);

	void WindowResized(Window* window);

	void MoveForward();
	void MoveBackward();
	void MoveLeft();
	void MoveRight();
	void MoveUp();
	void MoveDown();

	POINT mLastMousePos = {0,0};


	XMFLOAT4X4 mWorldMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mViewMatrix = MakeIdentity4x4();
	XMFLOAT4X4 mProjectionMatrix = MakeIdentity4x4();
	void UpdateSpeed(float speed);

	float mMovementSpeed = 1.0f;
	bool mInvertMouse = true;
	bool mOrbit = true;
	XMFLOAT3 mPos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mLastPos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	int mMoveLeftRight = 0;
	int mMoveBackForward = 0;
	int mMoveUpDown = 0;

	float mYaw = 0.0f;
	float mPitch = 0.0f;

	float NearZ = 0.01f;
	float FarZ = 2000.0f;

	float mWindowWidth = 0;
	float mWindowHeight = 0;

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;
};

