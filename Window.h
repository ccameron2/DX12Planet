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
	int mWidth;
	int mHeight;

};

