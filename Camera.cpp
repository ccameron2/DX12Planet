#include "Camera.h"

Camera::Camera(Window* window)
{
	// Set the aspect ratio and compute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(window->mWidth) / window->mHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProjectionMatrix, proj);
}

Camera::~Camera()
{
}

void Camera::MouseMoved(SDL_Event& event, Window* window)
{
	int mouseX, mouseY;
	mouseX = event.motion.x;
	mouseY = event.motion.y;

	if (window->mLeftMouse)
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
	else if (window->mRightMouse)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(mouseX - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(mouseY - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = std::clamp(mRadius, 0.1f, 90.0f);
	}
	mLastMousePos.x = mouseX;
	mLastMousePos.y = mouseY;
}

void Camera::Update()
{
	// Convert Spherical to Cartesian coordinates.
	mPos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mPos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mPos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mPos.x, mPos.y, mPos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mViewMatrix, view);
}

void Camera::WindowResized(Window* window)
{
	// The window resized, so update the aspect ratio and recompute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(window->mWidth) / window->mHeight, NearZ, FarZ);
	XMStoreFloat4x4(&mProjectionMatrix, proj);
}