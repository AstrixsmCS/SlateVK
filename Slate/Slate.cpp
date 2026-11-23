#include "Slate.hpp"

#include "Vulkan/Context.hpp"

#include <SDL3/SDL.h>

namespace Slate
{
	std::unique_ptr<Context> CreateContext(SDL_Window* window, const ContextConfig& config)
	{
		if (!window)
			return nullptr;

		auto context = std::make_unique<VulkanContext>(window, config);

		const auto physicalDevice = context->SelectPhysicalDevice();

		if (physicalDevice == VK_NULL_HANDLE)
			return nullptr;

		if (!context->FindGraphicsQueue())
			return nullptr;

		if (!context->CreateDevice(physicalDevice))
			return nullptr;

		if (!context->InitializeVMA())
			return nullptr;

		if (!context->CreateSwapChain(1600, 900))
			return nullptr;

		if (!context->CreateSyncResources())
			return nullptr;

		if (!context->CreateCommandBuffers())
			return nullptr;

		return context;
	}
}