#pragma once

#include <memory>

struct SDL_Window;

namespace Slate
{

	enum VulkanVersion
	{
		VulkanVersion_1_3 = 0,
		VulkanVersion_1_4,
	};

	struct ContextConfig
	{
		VulkanVersion Version = { VulkanVersion_1_3 };

		bool EnableValidation = true;
		bool EnableValidationGpu = true;
		bool EnableValidationSync = true;
		bool GenerateSPIRVDebugInfo = true;
	};

	class Context
	{
	public:
		virtual ~Context() = default;
	};

	std::unique_ptr<Context> CreateContext(SDL_Window* window, const ContextConfig& config = {});
}