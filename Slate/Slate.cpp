#include "Slate.hpp"

#include "Vulkan/Context.hpp"

namespace Slate
{
	std::unique_ptr<Context> CreateContext(SDL_Window* window, const ContextConfig& config)
	{
		if (!window)
			return nullptr;

		auto context = std::make_unique<VulkanContext>(window, config);

		/*const auto devices = context->QueryDevices();

		if (devices.empty())
			return nullptr;

		const VkPhysicalDevice physicalDevice = devices.front();

		if (context->InitializeContext(physicalDevice) != VK_SUCCESS)
			return nullptr;

		if (context->InitializeSwapChain() != VK_SUCCESS)
			return nullptr;*/

		return context;
	}
}