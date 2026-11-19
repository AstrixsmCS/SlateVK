#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <memory>

struct SDL_Window;
union  SDL_Event;

enum class WindowMode: uint8_t
{
	Windowed = 0,
	BorderlessFullscreen,
	ExclusiveFullscreen
};

struct WindowSpecification
{
	std::string Title { "SlateVK" };
	int32_t Width    { 1600 };
	int32_t Height   { 900 };

	WindowMode Mode   { WindowMode::Windowed };

	bool Resizable    { true };
};

class SlateApp
{
public:
	explicit SlateApp(const WindowSpecification& specification);
	~SlateApp();

	SlateApp(const SlateApp&)            = delete;
	SlateApp& operator=(const SlateApp&) = delete;

	void ProcessEvents();

	bool IsRunning() const { return m_Running; }

	std::pair<int32_t, int32_t>     GetSize() const;
	std::pair<int32_t, int32_t>     GetWindowPos() const;

	// Window attributes
	void SetResizable(bool resizable);

	void Maximize();
	void CenterWindow();

	const std::string& GetTitle() const { return m_Specification.Title; }
	void               SetTitle(const std::string& title);

	int32_t GetWidth() const { return GetSize().first; }
	int32_t GetHeight() const { return GetSize().second; }

	SDL_Window* GetNativeWindow() const { return m_Window; }

	static double GetTime();
private:
	SDL_Window* m_Window { nullptr };

	WindowSpecification m_Specification;

	bool m_Running { false };
};
