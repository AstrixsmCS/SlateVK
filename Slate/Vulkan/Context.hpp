#pragma once

#include "Slate/Slate.hpp"

#include <volk.h>
#include "vk_mem_alloc.h"

#include <vector>

namespace Slate
{
	class VulkanContext final : public Context
	{
	public:
		VulkanContext(SDL_Window* window, const ContextConfig& config = {});
		~VulkanContext() override;

		VkPhysicalDevice SelectPhysicalDevice();
		bool FindGraphicsQueue();

		bool CreateDevice(VkPhysicalDevice physicalDevice);
		bool InitializeVMA();
		bool CreateSwapChain(uint32_t width, uint32_t height);
	private:
		SDL_Window* m_Window = { nullptr };
		ContextConfig m_Config {};

		VkInstance m_Instance = { VK_NULL_HANDLE };
		VkDebugUtilsMessengerEXT m_DebugMessenger = { VK_NULL_HANDLE };
		VkSurfaceKHR m_Surface = { VK_NULL_HANDLE};

		VkPhysicalDevice m_PhysicalDevice = { VK_NULL_HANDLE };
		VkDevice m_Device = { VK_NULL_HANDLE };

		uint32_t m_GraphicsQueueFamilyIndex = UINT32_MAX;
		VkQueue m_GraphicsQueue = { VK_NULL_HANDLE };

		VmaAllocator m_VmaAllocator = { VK_NULL_HANDLE };
	};
}
