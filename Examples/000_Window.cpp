#include "SlateApp.hpp"

#include <Slate/Slate.hpp>

int main()
{
	SlateApp app({
		.Title = "SlateVK - Window",
		.Width = 1600,
		.Height = 900,
		.Mode = WindowMode::Windowed,
		.Resizable = true
	});

	if (!app.IsRunning())
		return 1;

	app.CenterWindow();

	Slate::ContextConfig config
	{
		.Version = Slate::VulkanVersion::VulkanVersion_1_4,
		.EnableValidationGpu = false
	};

	auto context = Slate::CreateContext(app.GetNativeWindow(), config);

	if (!context)
		return -1;

	while (app.IsRunning())
	{
		app.ProcessEvents();
	}

	return 0;
}
