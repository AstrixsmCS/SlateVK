#include "Context.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <SDL3/SDL_Vulkan.h>

#include <print>

namespace Slate
{
	VulkanContext::VulkanContext(SDL_Window* window, const ContextConfig& config)
		: m_Window(window), m_Config(config)
	{
		auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());

		if (!getInstanceProcAddr)
		{
			std::println(stderr, "Failed to get Vulkan loader: {}", SDL_GetError());
			return;
		}

		volkInitializeCustom(getInstanceProcAddr);

		uint32_t sdlExtensionCount = 0;
		const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
		std::vector<const char*> requestedExtensions
		{
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME
		};
		for (int i = 0; i < sdlExtensionCount; ++i)
		{
			requestedExtensions.push_back(sdlExtensions[i]);
		}

		//std::vector<const char*> requestedExtensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

		std::vector<const char*> requestedLayers
		{
			"VK_LAYER_KHRONOS_validation"
		};

		const VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "SlateVK",
			.apiVersion = VK_API_VERSION_1_4
		};

		const VkInstanceCreateInfo instanceCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
			.ppEnabledLayerNames = requestedLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size()),
			.ppEnabledExtensionNames = requestedExtensions.data()
		};

		if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance) != VK_SUCCESS)
		{
			std::println(stderr, "Failed to create Vulkan instance!");
			return;
		}

		volkLoadInstance(m_Instance);

		if (!SDL_Vulkan_CreateSurface(m_Window, m_Instance, nullptr, &m_Surface))
		{
			std::println(stderr, "Failed to create Vulkan surface: {}", SDL_GetError());
			return;
		}

		std::println("Vulkan instance created successfully.");
	}

	VulkanContext::~VulkanContext()
	{
		// TODO: Destroy swapchain and associated resources.

		if (m_Device != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(m_Device);
			vkDestroyDevice(m_Device, nullptr);
		}

		if (m_Surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		}

		if (m_DebugMessenger != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
		}

		if (m_Instance != VK_NULL_HANDLE)
		{
			vkDestroyInstance(m_Instance, nullptr);
		}

		volkFinalize();
	}

	std::vector<VkPhysicalDevice> VulkanContext::QueryDevices() const
	{
		// VkEnumeratePhysicalDevices()
		return {};
	}

	VkResult VulkanContext::InitializeContext(VkPhysicalDevice physicalDevice)
	{
		m_PhysicalDevice = physicalDevice;

		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkResult VulkanContext::InitializeSwapChain()
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}
}