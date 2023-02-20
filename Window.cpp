#include "Window.h"

Window::Window(int width, int height)
{
	SDL_Init(SDL_INIT_VIDEO);
	mWidth = width;
	mHeight = height;
	mSDLWindow = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mWidth, mHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	SetWindowTitle();
}

Window::~Window()
{
	// Shutdown SDL
	SDL_DestroyWindow(mSDLWindow);
	SDL_Quit();
}

void Window::SetWindowTitle()
{
	// Set window text to title
	std::string windowText = mMainCaption;
	const char* array = windowText.c_str();
	SDL_SetWindowTitle(mSDLWindow, array);
}

void Window::ProcessEvents(SDL_Event& event)
{
	if (event.type == SDL_WINDOWEVENT)
	{
		switch (event.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			mWidth = event.window.data1;
			mHeight = event.window.data2;
			mResized = true;
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			break;
		case SDL_WINDOWEVENT_ENTER:
			mMouseFocus = true;
			break;
		case SDL_WINDOWEVENT_LEAVE:
			mMouseFocus = false;
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			mKeyboardFocus = true;
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			mKeyboardFocus = false;
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			mMinimized = true;
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			mMinimized = false;
			break;
		case SDL_WINDOWEVENT_RESTORED:
			mMinimized = false;
			break;
		}
	}
	else if (event.type == SDL_KEYDOWN)
	{
		auto key = event.key.keysym.sym;
		if (key == SDLK_F11)
		{
			if (mFullscreen)
			{
				SDL_SetWindowFullscreen(mSDLWindow, SDL_FALSE);
			}
			else
			{
				SDL_SetWindowFullscreen(mSDLWindow, SDL_TRUE);
			}
			mFullscreen = !mFullscreen;
		}
	}
	else if (event.type == SDL_MOUSEMOTION)
	{
		mMouseMoved = true;
	}
	else if (event.type == SDL_QUIT)
	{
		mQuit = true;
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN)
	{
		if (event.button.button == 3) { mRightMouse = true; }
		else if (event.button.button == 1) { mLeftMouse = true; }
		else if (event.button.button == 2) { mMiddleMouse = true; }
	}
	else if (event.type == SDL_MOUSEBUTTONUP)
	{
		if (event.button.button == 3) { mRightMouse = false; }
		else if (event.button.button == 1) { mLeftMouse = false; }
		else if (event.button.button == 2) { mMiddleMouse = false; }
	}
}

HWND Window::GetHWND()
{
	SDL_SysWMinfo info;
	SDL_GetVersion(&info.version);
	SDL_GetWindowWMInfo(mSDLWindow, &info);
	HWND hwnd = info.info.win.window;
	return hwnd;
}
