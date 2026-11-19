#pragma once

#include "Slate/Slate.hpp"

#include <volk.h>

#include <vector>

namespace Slate
{
	class VulkanContext final : public Context
	{
	public:
		VulkanContext(SDL_Window* window, const ContextConfig& config = {});
		~VulkanContext() override;

		std::vector<VkPhysicalDevice> QueryDevices() const;

		VkResult InitializeContext(VkPhysicalDevice physicalDevice);
		VkResult InitializeSwapChain();
	private:
		SDL_Window* m_Window = { nullptr };
		ContextConfig m_Config {};

		VkInstance m_Instance = { VK_NULL_HANDLE };
		VkDebugUtilsMessengerEXT m_DebugMessenger =  { VK_NULL_HANDLE };
		VkSurfaceKHR m_Surface = { VK_NULL_HANDLE};

		VkPhysicalDevice m_PhysicalDevice = { VK_NULL_HANDLE };
		VkDevice m_Device = { VK_NULL_HANDLE };
	};
}
