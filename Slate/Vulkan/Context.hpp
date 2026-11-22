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
		void DestroySwapChain();
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

		VkSwapchainKHR m_Swapchain = { VK_NULL_HANDLE };
		VkFormat m_SwapchainFormat = { VK_FORMAT_B8G8R8A8_SRGB };
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;
		std::vector<VkSemaphore> m_RenderCompleteSemaphores;
		uint32_t m_SwapchainWidth { 0 };
		uint32_t m_SwapchainHeight { 0 };

		VkImage m_DepthImage = { VK_NULL_HANDLE };
		VkImageView m_DepthImageView = { VK_NULL_HANDLE };
		VkFormat m_DepthFormat = { VK_FORMAT_D32_SFLOAT };
		VmaAllocation m_DepthImageAllocation = { VK_NULL_HANDLE };
	};
}
