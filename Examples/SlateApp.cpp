#include "SlateApp.hpp"

#include <print>

#include <SDL3/SDL.h>

static void SDLErrorCallback(const char* context)
{
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL Error (%s): %s", context, SDL_GetError());
}

SlateApp::SlateApp(const WindowSpecification& specification)
	: m_Specification(specification)
{
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		SDLErrorCallback("SDL_InitSubSystem");
		return;
	}

	SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

	if (specification.Resizable)
		flags |= SDL_WINDOW_RESIZABLE;

	m_Window = SDL_CreateWindow(specification.Title.c_str(), specification.Width, specification.Height, flags);

	if (!m_Window)
	{
		SDLErrorCallback("SDL_CreateWindow");
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return;
	}

	// Window mode
	bool success = true;

	switch (specification.Mode)
	{
	case WindowMode::Windowed:
		break;

	case WindowMode::BorderlessFullscreen:
	{
			success = SDL_SetWindowFullscreenMode(m_Window, nullptr) && SDL_SetWindowFullscreen(m_Window, true);
			break;
	}

	case WindowMode::ExclusiveFullscreen:
	{
		SDL_DisplayID display = SDL_GetDisplayForWindow(m_Window);
		SDL_DisplayMode mode = {};

		success = SDL_GetClosestFullscreenDisplayMode(
			display,
			specification.Width,
			specification.Height,
			0.0f,
			true,
			&mode
		);

		if (success)
		{
			success = SDL_SetWindowFullscreenMode(m_Window, &mode) && SDL_SetWindowFullscreen(m_Window, true);
		}

		break;
	}
	}

	if (!success)
	{
		SDLErrorCallback("Window mode");

		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return;
	}

	m_Running = true;
}

SlateApp::~SlateApp()
{
	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

void SlateApp::ProcessEvents()
{
	if (!m_Window)
		return;

	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window)))
		{
			m_Running = false;
		}
	}
}

std::pair<int32_t, int32_t> SlateApp::GetSize() const
{
	int width = 0;
	int height = 0;

	if (!m_Window)
		return { 0, 0 };

	if (!SDL_GetWindowSize(m_Window, &width, &height))
	{
		SDLErrorCallback("SDL_GetWindowSize");
		return { 0, 0 };
	}

	return { width, height };
}

std::pair<int32_t, int32_t> SlateApp::GetWindowPos() const
{
	int x = 0;
	int y = 0;

	if (!m_Window)
		return { 0, 0 };

	if (!SDL_GetWindowPosition(m_Window, &x, &y))
		SDLErrorCallback("SDL_GetWindowPosition");

	return { x, y };
}

void SlateApp::SetResizable(bool resizable)
{
	if (!m_Window)
		return;

	if (!SDL_SetWindowResizable(m_Window, resizable))
	{
		SDLErrorCallback("SDL_SetWindowResizable");
		return;
	}

	m_Specification.Resizable = resizable;
}

void SlateApp::Maximize()
{
	if (m_Window && !SDL_MaximizeWindow(m_Window))
		SDLErrorCallback("SDL_MaximizeWindow");
}

void SlateApp::CenterWindow()
{
	if (!m_Window)
		return;

	if (!SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED))
	{
		SDLErrorCallback("SDL_SetWindowPosition");
	}
}

void SlateApp::SetTitle(const std::string& title)
{
	if (!m_Window)
		return;

	if (!SDL_SetWindowTitle(m_Window, title.c_str()))
	{
		SDLErrorCallback("SDL_SetWindowTitle");
		return;
	}

	m_Specification.Title = title;
}

double SlateApp::GetTime()
{
	return static_cast<double>(SDL_GetTicksNS()) / 1'000'000'000.0;
}