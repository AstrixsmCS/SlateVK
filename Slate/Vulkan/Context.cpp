#include "Context.hpp"

#include "Vulkan.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

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
			SDL_Vulkan_DestroySurface(m_Instance, m_Surface, nullptr);
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

	VkPhysicalDevice VulkanContext::SelectPhysicalDevice() const
	{
		uint32_t deviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));

		if (deviceCount == 0)
			return VK_NULL_HANDLE;

		std::vector<VkPhysicalDevice> devices(deviceCount);
		VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()));

		VkPhysicalDevice selectedDevice = nullptr;
		if (deviceCount)
		{
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
		}

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(selectedDevice, &properties);
		std::println("Selected Physical Device: {}", properties.deviceName);

		return selectedDevice;
	}

	bool VulkanContext::CreateDevice(VkPhysicalDevice physicalDevice)
	{
		return false;
	}

	bool VulkanContext::CreateSwapChain()
	{
		return false;
	}
}