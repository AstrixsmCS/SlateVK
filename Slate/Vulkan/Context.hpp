#pragma once

#include "Slate/Slate.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <array>

namespace Slate
{
	struct FrameResources
	{
		VkCommandPool commandPool = { VK_NULL_HANDLE };
		VkCommandBuffer commandBuffer = { VK_NULL_HANDLE };
		VkSemaphore imageAcquiredSemaphore = { VK_NULL_HANDLE };
	};

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

		VkShaderModule CreateShaderModule(const char* source, const char* entryPointName);
		VkPipeline CreateGraphicsPipeline(const GraphicsPipelineSpecification& spec) override;

		bool CreateSyncResources();
		bool CreateCommandBuffers();

		void Render(const float* mvp) override;
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
		bool m_RequireSwapchainRecreate = false;
		uint32_t m_SwapchainWidth { 0 };
		uint32_t m_SwapchainHeight { 0 };

		VkImage m_DepthImage = { VK_NULL_HANDLE };
		VkImageView m_DepthImageView = { VK_NULL_HANDLE };
		VkFormat m_DepthFormat = { VK_FORMAT_D32_SFLOAT };
		VmaAllocation m_DepthImageAllocation = { VK_NULL_HANDLE };

		VkPipelineLayout m_PipelineLayout = { VK_NULL_HANDLE };
		VkPipeline m_GraphicsPipeline = { VK_NULL_HANDLE };

		VkSemaphore m_TimelineSemaphore = { VK_NULL_HANDLE };
		constexpr static uint32_t MaxFramesInFlight{ 3 };
		std::array<FrameResources, MaxFramesInFlight> m_FrameResources = {};

		uint64_t frameIndex { 0 };
		uint64_t nextSignalValue = MaxFramesInFlight + 1;
	};
}
