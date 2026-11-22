#include "Context.hpp"

#include "Vulkan.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <SDL3/SDL_Vulkan.h>

#include <print>

namespace Slate
{
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		[[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		[[maybe_unused]] void* pUserData)
	{
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			std::println("Validation Layer: {}", pCallbackData->pMessage);
		}

		return VK_FALSE;
	}

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

		if (!sdlExtensions)
		{
			std::println(stderr, "Failed to get Vulkan extensions: {}", SDL_GetError());
			return;
		}

		std::vector<const char*> requestedExtensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
		std::vector<const char*> requestedLayers;

		if (config.EnableValidation)
		{
			requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			requestedExtensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
			requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
		}

		for (const char* extension : requestedExtensions)
			std::println("Instance Extension: {}", extension);

		const VkApplicationInfo applicationInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "SlateVK",
			.apiVersion = config.Version == VulkanVersion_1_4 ? VK_API_VERSION_1_4 : VK_API_VERSION_1_3
		};

		const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
		};

		const VkBool32 enableGpuValidation = config.EnableValidationGpu ? VK_TRUE : VK_FALSE;
		const VkBool32 enableSyncValidation = config.EnableValidationSync ? VK_TRUE : VK_FALSE;

		const VkLayerSettingEXT layerSettings[]
		{
			{
				.pLayerName = "VK_LAYER_KHRONOS_validation",
				.pSettingName = "gpuav_enable",
				.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
				.valueCount = 1,
				.pValues = &enableGpuValidation,

			},
			{
				.pLayerName = "VK_LAYER_KHRONOS_validation",
				.pSettingName = "validate_sync",
				.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
				.valueCount = 1,
				.pValues = &enableSyncValidation,
			}
		};

		const VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
			.pNext = &debugCreateInfo,
			.settingCount = 2,
			.pSettings = layerSettings
		};

		const VkInstanceCreateInfo instanceCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = config.EnableValidation ? &layerSettingsCreateInfo : nullptr,
			.pApplicationInfo = &applicationInfo,
			.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
			.ppEnabledLayerNames = requestedLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size()),
			.ppEnabledExtensionNames = requestedExtensions.data()
		};

		VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance));

		volkLoadInstance(m_Instance);

		if (config.EnableValidation)
		{
			VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_Instance, &debugCreateInfo, nullptr, &m_DebugMessenger));
		}

		if (!SDL_Vulkan_CreateSurface(m_Window, m_Instance, nullptr, &m_Surface))
		{
			std::println(stderr, "Failed to create Vulkan surface: {}", SDL_GetError());
			return;
		}
	}

	VulkanContext::~VulkanContext()
	{
		// TODO: Destroy swapchain and associated resources.

		if (m_Device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(m_Device);

		if (m_Device != VK_NULL_HANDLE)
			DestroySwapChain();

		if (m_VmaAllocator != VK_NULL_HANDLE)
			vmaDestroyAllocator(m_VmaAllocator);

		if (m_Device != VK_NULL_HANDLE)
			vkDestroyDevice(m_Device, nullptr);

		if (m_Surface != VK_NULL_HANDLE)
			SDL_Vulkan_DestroySurface(m_Instance, m_Surface, nullptr);

		if (m_DebugMessenger != VK_NULL_HANDLE)
			vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

		if (m_Instance != VK_NULL_HANDLE)
			vkDestroyInstance(m_Instance, nullptr);

		volkFinalize();
	}

	VkPhysicalDevice VulkanContext::SelectPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));

		if (deviceCount == 0)
			return VK_NULL_HANDLE;

		std::vector<VkPhysicalDevice> devices(deviceCount);
		VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()));

		VkPhysicalDevice selectedDevice = nullptr;

		selectedDevice = devices[0];

		for (auto &device : devices)
		{
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(device, &properties);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				selectedDevice = device;
				break;
			}
		}

		m_PhysicalDevice = selectedDevice;

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, surfaceFormats.data());

		bool formatSupported = false;
		for (const VkSurfaceFormatKHR &surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == m_SwapchainFormat && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				formatSupported = true;
				break;
			}
		}
		if (!formatSupported)
		{
			std::println("Requested format is not supported by the surface");
			return nullptr;
		}

		return m_PhysicalDevice;
	}

	bool VulkanContext::FindGraphicsQueue()
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties2(m_PhysicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount, { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
		vkGetPhysicalDeviceQueueFamilyProperties2(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

		for (uint32_t familyIndex = 0; familyIndex < queueFamilies.size(); familyIndex++)
		{
			VkBool32 supportsPresent = VK_FALSE;
			VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, familyIndex, m_Surface, &supportsPresent));

			const auto &properties = queueFamilies[familyIndex];

			if (properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && supportsPresent)
			{
				m_GraphicsQueueFamilyIndex = familyIndex;
				return true;
			}
		}

		return false;
	}

	bool VulkanContext::CreateDevice(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceVulkan14Features supportedFeatures14
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr };
		VkPhysicalDeviceVulkan13Features supportedFeatures13
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &supportedFeatures14 };
		VkPhysicalDeviceVulkan12Features supportedFeatures12
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &supportedFeatures13 };
		VkPhysicalDeviceFeatures2 supportedFeatures
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supportedFeatures12 };
		vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures);

		if (!supportedFeatures13.dynamicRendering || !supportedFeatures13.synchronization2 || !supportedFeatures12.timelineSemaphore)
		{
			std::println("Physical device doesn't meet the feature requirments");
			return false;
		}

		VkPhysicalDeviceVulkan14Features features14
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
			.pNext = nullptr,
		};
		VkPhysicalDeviceVulkan13Features features13
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &features14,
			.synchronization2 = VK_TRUE,
			.dynamicRendering = VK_TRUE,
		};
		VkPhysicalDeviceVulkan12Features features12
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &features13,
			.timelineSemaphore = VK_TRUE,
		};
		VkPhysicalDeviceFeatures2 features
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features12 };

		std::vector<float> queuePriorities{ 1.0f };
		VkDeviceQueueCreateInfo GraphicsQueueInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = m_GraphicsQueueFamilyIndex,
			.queueCount = 1,
			.pQueuePriorities = queuePriorities.data(),
		};

		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		std::println("Physical device '{}' has {} extensions: ", properties.deviceName, extensions.size());

		for (const auto& extension : extensions)
		{
			std::println("{} ", extension.extensionName);
		}

		const std::vector<const char*> deviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo deviceCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &GraphicsQueueInfo,
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
			.pEnabledFeatures = nullptr, // features struct chain is set pNext
		};

		VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &m_Device));

		volkLoadDevice(m_Device);

		vkGetDeviceQueue(m_Device, m_GraphicsQueueFamilyIndex, 0, &m_GraphicsQueue);

		if (!m_GraphicsQueue)
		{
			std::println("Couldn't get the graphics queue.");
			return false;
		}

		return true;
	}

	bool VulkanContext::InitializeVMA()
	{
		VmaVulkanFunctions vmaFunctions{};

		VmaAllocatorCreateInfo allocatorCreateInfo
		{
			.flags = 0,
			.physicalDevice = m_PhysicalDevice,
			.device = m_Device,
			.pVulkanFunctions = &vmaFunctions,
			.instance = m_Instance,
			.vulkanApiVersion = m_Config.Version == VulkanVersion_1_4 ? VK_API_VERSION_1_4 : VK_API_VERSION_1_3,
		};

		VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocatorCreateInfo, &vmaFunctions));

		VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_VmaAllocator));

		return true;
	}

	bool VulkanContext::CreateSwapChain(uint32_t width, uint32_t height)
	{
		m_SwapchainWidth = width;
		m_SwapchainHeight = height;

		VkSurfaceCapabilitiesKHR surfaceCaps{};

		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &surfaceCaps));

		uint32_t requestedImageCount = std::max(2u, surfaceCaps.minImageCount);
		if (surfaceCaps.maxImageCount > 0)
		{
			requestedImageCount = std::min(requestedImageCount, surfaceCaps.maxImageCount);
		}

		VkSwapchainCreateInfoKHR swapchainCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = m_Surface,
			.minImageCount = requestedImageCount,
			.imageFormat = m_SwapchainFormat,
			.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			.imageExtent = { .width = m_SwapchainWidth, .height = m_SwapchainHeight },
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = surfaceCaps.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_FIFO_KHR
		};

		VK_CHECK(vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, nullptr, &m_Swapchain));

		uint32_t imageCount = 0;
		VK_CHECK(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr));
		m_SwapchainImages.resize(imageCount);
		VK_CHECK(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data()));
		m_SwapchainImageViews.resize(imageCount);

		for (uint32_t i = 0; i < m_SwapchainImages.size(); ++i)
		{
			VkImageViewCreateInfo imageViewCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = m_SwapchainImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = m_SwapchainFormat,
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			VK_CHECK(vkCreateImageView(m_Device, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews[i]));
		}

		m_RenderCompleteSemaphores.resize(m_SwapchainImages.size());
		for (VkSemaphore &semaphore : m_RenderCompleteSemaphores)
		{
			VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &semaphore));
		}

		VkImageCreateInfo depthCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = m_DepthFormat,
			.extent = { .width = m_SwapchainWidth, .height = m_SwapchainHeight, .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};

		VmaAllocationCreateInfo allocationCreateInfo
		{
			.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VK_CHECK(vmaCreateImage(m_VmaAllocator, &depthCreateInfo, &allocationCreateInfo, &m_DepthImage, &m_DepthImageAllocation, nullptr));

		VkImageViewCreateInfo depthImgViewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_DepthImage,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_DepthFormat,
			.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
		};

		VK_CHECK(vkCreateImageView(m_Device, &depthImgViewCreateInfo, nullptr, &m_DepthImageView));

		return true;
	}

	void VulkanContext::DestroySwapChain()
	{
		for (VkImageView swapchainImageView : m_SwapchainImageViews)
		{
			vkDestroyImageView(m_Device, swapchainImageView, nullptr);
		}
		m_SwapchainImageViews.clear();

		for (VkSemaphore &semaphore : m_RenderCompleteSemaphores)
		{
			vkDestroySemaphore(m_Device, semaphore, nullptr);
		}
		m_RenderCompleteSemaphores.clear();

		if (m_Swapchain)
		{
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
			m_Swapchain = VK_NULL_HANDLE;
		}

		if (m_DepthImageView)
		{
			vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
			vmaDestroyImage(m_VmaAllocator, m_DepthImage, m_DepthImageAllocation);
			m_DepthImageView = VK_NULL_HANDLE;
		}
	}
}