#pragma once
#include <windows.h>

#include "SDL.h"
#include <sdl_syswm.h>

#include <string>;

class SDL2Window
{
private:
	void SetWindowTitle();
public:
	SDL2Window(int width, int height);
	~SDL2Window();
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

