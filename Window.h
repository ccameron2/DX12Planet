#pragma once
#include <windows.h>

#include "SDL.h"
#include <sdl_syswm.h>

#include <string>;

class Window
{
private:
	void SetWindowTitle();
public:
	Window(int width, int height);
	~Window();
	
	void ProcessEvents(SDL_Event& event);
	HWND GetHWND();

	SDL_Window* mSDLWindow;
	std::string mMainCaption = "D3D12 Engine Masters";

	// Input flags
	bool mAppPaused = false;
	bool mResized = false;
	bool mFullscreen = false;
	bool mRightMouse = false;
	bool mLeftMouse = false;
	bool mMiddleMouse = false;
	bool mMouseFocus = false;
	bool mMouseMoved = false;
	bool mKeyboardFocus = false;
	bool mMinimized = false;
	bool mQuit = false;
	bool mForward = false;
	bool mBackward = false;
	bool mLeft = false;
	bool mRight = false;
	bool mUp = false;
	bool mDown = false;
	bool mWireframe = false;
	float mScrollValue = 0.0f;

	// Window resolution
	int mWidth;
	int mHeight;

};

