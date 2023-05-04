#include "Camera.h"

Camera::Camera(Window* window)
{
	// Set the aspect ratio and compute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(window->mWidth) / window->mHeight, NearZ, FarZ);
	XMStoreFloat4x4(&mProjectionMatrix, proj);

	mWindowWidth = window->mWidth;
	mWindowHeight = window->mHeight;
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

		if(mOrbit)
		{
			// Update angles based on input to orbit camera around box.
			mTheta += dx;
			mPhi += dy;

			// Restrict the angle mPhi.
			mPhi = std::clamp(mPhi, 0.1f, XM_PI - 0.1f);
		}
		else
		{
			// Update the camera's yaw and pitch based on mouse movement
			float dx = XMConvertToRadians(0.25f * static_cast<float>(mouseX - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(mouseY - mLastMousePos.y));

			mYaw += dx;
			if (mInvertMouse) mPitch += dy;
			else mPitch -= dy;

			// Clamp the pitch to avoid looking upside down
			mPitch = std::clamp(mPitch, -XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f);
		}
	}
	else if (window->mRightMouse)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(mouseX - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(mouseY - mLastMousePos.y);

		if (mOrbit)
		{
			// Update the camera radius based on input.
			mRadius += dx - dy;

			// Restrict the radius.
			mRadius = std::clamp(mRadius, 0.1f, 8000.0f);
		}
	}
	mLastMousePos.x = mouseX;
	mLastMousePos.y = mouseY;
}

void Camera::Update(float frameTime, bool orbit, bool invertMouse, float speedMultiplier)
{
	mInvertMouse = invertMouse;
	mOrbit = orbit;
	if (mOrbit)
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
	else
	{
		// Calculate the camera's position in world space.
		float cosPitch = cosf(mPitch);
		XMFLOAT3 forward(cosPitch * sinf(mYaw), sinf(mPitch), cosPitch * cosf(mYaw));
		XMVECTOR forwardVector = XMLoadFloat3(&forward);
		XMVECTOR rightVector = XMVector3Cross(forwardVector, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		XMVECTOR upVector = XMVector3Cross(rightVector, forwardVector);
		XMVECTOR positionVector = XMLoadFloat3(&mPos);
		positionVector += mMoveLeftRight * frameTime * mMovementSpeed * speedMultiplier * rightVector;
		positionVector += mMoveBackForward * frameTime * mMovementSpeed * speedMultiplier * forwardVector;
		positionVector += mMoveUpDown * frameTime * mMovementSpeed * speedMultiplier * upVector;
		XMStoreFloat3(&mPos, positionVector);

		mMoveLeftRight = 0;
		mMoveBackForward = 0;
		mMoveUpDown = 0;

		// Build the view matrix.
		XMVECTOR target = positionVector + forwardVector;
		XMVECTOR up = upVector;
		XMMATRIX view = XMMatrixLookAtLH(positionVector, target, up);
		XMStoreFloat4x4(&mViewMatrix, view);
	}
	
}

void Camera::WindowResized(Window* window)
{
	// The window resized, so update the aspect ratio and recompute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(window->mWidth) / window->mHeight, NearZ, FarZ);
	XMStoreFloat4x4(&mProjectionMatrix, proj);

	mWindowWidth = window->mWidth;
	mWindowHeight = window->mHeight;
}

void Camera::MoveForward()
{
	mMoveBackForward = 1.0f;
}

void Camera::MoveBackward()
{
	mMoveBackForward = -1.0f;
}

void Camera::MoveLeft()
{
	mMoveLeftRight = 1.0f;
}

void Camera::MoveRight()
{
	mMoveLeftRight = -1.0f;
}

void Camera::MoveUp()
{
	mMoveUpDown = 1.0f;
}

void Camera::MoveDown()
{
	mMoveUpDown = -1.0f;
}

void Camera::UpdateSpeed(float speed)
{
	if (mMovementSpeed + speed > 0)
	{
		mMovementSpeed += speed;
	}

}
