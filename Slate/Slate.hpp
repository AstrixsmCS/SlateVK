#pragma once

#include <memory>

#include <volk.h>

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

		virtual VkPipeline CreateGraphicsPipeline(const char* vertexShader, const char* fragmentShader) = 0;
		virtual void Render(const float* mvp) = 0;
	};

	std::unique_ptr<Context> CreateContext(SDL_Window* window, const ContextConfig& config = {});
}