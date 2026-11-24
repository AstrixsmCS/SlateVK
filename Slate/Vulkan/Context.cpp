#include "Context.hpp"

#include "Vulkan.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <slang.h>
#include <slang-com-ptr.h>

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
		// let SDL hand us the vulkan loader, then initialize volk with it
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

		// layer settings toggle gpu-assisted and sync validation
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
		// wait in case resources are in use
		if (m_Device != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(m_Device);

			if (m_GraphicsPipeline != VK_NULL_HANDLE)
				vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);

			if (m_PipelineLayout != VK_NULL_HANDLE)
				vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

			// destroying the pool destroys its buffers implicitly
			for (auto &res : m_FrameResources)
			{
				vkDestroyCommandPool(m_Device, res.commandPool, nullptr);
				vkDestroySemaphore(m_Device, res.imageAcquiredSemaphore, nullptr);
			}

			if (m_TimelineSemaphore != VK_NULL_HANDLE)
				vkDestroySemaphore(m_Device, m_TimelineSemaphore, nullptr);

			DestroySwapChain();
		}

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

		// default to first GPU, then look through the list and see if a dGPU exists
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

		// ensure the desired swapchain format is supported
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

			// ensure this is a GRAPHICS queue with presentation support
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
		// query supported features
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

		// produce a separate features struct chain for device creation
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

		// list the device extensions, handy for debugging
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

		// vma can import directly from volk
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

		// ask for at least two images, but don't go over the surfaces max
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

		// one rendercomplete semaphore per swapchain image
		m_RenderCompleteSemaphores.resize(m_SwapchainImages.size());
		for (VkSemaphore &semaphore : m_RenderCompleteSemaphores)
		{
			VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &semaphore));
		}

		// create depth image
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

		// destroy the depth buffer along with the swapchain
		if (m_DepthImageView)
		{
			vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
			vmaDestroyImage(m_VmaAllocator, m_DepthImage, m_DepthImageAllocation);
			m_DepthImageView = VK_NULL_HANDLE;
		}
	}

	VkShaderModule VulkanContext::CreateShaderModule(const char* source, const char* entryPointName)
	{
		// std::println("Compiling shader '{}':\n{}", entryPointName, source); // For debugging

		Slang::ComPtr<slang::IGlobalSession> globalSession;
		if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())))
			return VK_NULL_HANDLE;

		// target SPIR-V for Vulkan
		slang::TargetDesc target
		{
			.format = SLANG_SPIRV,
			.profile = globalSession->findProfile("spirv_1_6")
		};

		slang::SessionDesc sessionDesc
		{
			.targets = &target,
			.targetCount = 1,
			.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR
		};

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())))
			return VK_NULL_HANDLE;

		Slang::ComPtr<slang::IBlob> diagnostics;

		auto module = session->loadModuleFromSourceString("Shader", "Shader.slang", source, diagnostics.writeRef());

		if (diagnostics)
			std::println(stderr, "{}", static_cast<const char*>(diagnostics->getBufferPointer()));

		if (!module)
			return VK_NULL_HANDLE;

		Slang::ComPtr<slang::IEntryPoint> entryPoint;
		if (SLANG_FAILED(module->findEntryPointByName(entryPointName, entryPoint.writeRef())))
			return VK_NULL_HANDLE;

		// combine the module and entry point into one program, then link it
		slang::IComponentType* components[] = { module, entryPoint };

		Slang::ComPtr<slang::IComponentType> program;
		if (SLANG_FAILED(session->createCompositeComponentType(components, 2, program.writeRef(), diagnostics.writeRef())))
			return VK_NULL_HANDLE;

		Slang::ComPtr<slang::IComponentType> linkedProgram;
		if (SLANG_FAILED(program->link(linkedProgram.writeRef(), diagnostics.writeRef())))
			return VK_NULL_HANDLE;

		Slang::ComPtr<slang::IBlob> spirv;
		if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, spirv.writeRef(), diagnostics.writeRef())))
			return VK_NULL_HANDLE;

		// pass spir-v to vulkan and create the shader module
		VkShaderModuleCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = spirv->getBufferSize(),
			.pCode = static_cast<const uint32_t*>(spirv->getBufferPointer())
		};

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VK_CHECK(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule));

		return shaderModule;
	}

	VkPipeline VulkanContext::CreateGraphicsPipeline(const GraphicsPipelineSpecification& spec)
	{
		VkPushConstantRange pushConstantRange
		{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(float) * 16
		};

		// empty pipeline layout for now, no descriptors or push constants
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 0,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushConstantRange
		};

		VK_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout));

		VkShaderModule vertexModule = CreateShaderModule(spec.VertexShader, "vertexMain");
		VkShaderModule fragmentModule = CreateShaderModule(spec.FragmentShader, "fragmentMain");

		if (vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE)
		{
			if (vertexModule != VK_NULL_HANDLE)
				vkDestroyShaderModule(m_Device, vertexModule, nullptr);

			if (fragmentModule != VK_NULL_HANDLE)
				vkDestroyShaderModule(m_Device, fragmentModule, nullptr);

			return VK_NULL_HANDLE;
		}

		const VkPipelineShaderStageCreateInfo shaderStages[]
		{
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertexModule,
				.pName = "main"
			},
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragmentModule,
				.pName = "main"
			}
		};

		// vertex pulling, don't define vertex input details
		VkPipelineVertexInputStateCreateInfo vertInputInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		};

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.stencilTestEnable = VK_FALSE,
		};

		// viewport and scissor are set dynamically, we still need this struct though
		VkPipelineViewportStateCreateInfo viewportInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr,
		};

		VkPipelineRasterizationStateCreateInfo rasterInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f,
		};

		VkPipelineMultisampleStateCreateInfo multisampleInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		};

		// blending is disabled for now, still need attachment info and write mask
		VkPipelineColorBlendAttachmentState attachmentState
		{
			.blendEnable = VK_FALSE,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo blendInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &attachmentState,
		};

		std::vector<VkDynamicState> dynamicState
		{
			VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
			.pDynamicStates = dynamicState.data(),
		};

		// structure required for dynamic rendering
		VkPipelineRenderingCreateInfo renderInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &m_SwapchainFormat,
			.depthAttachmentFormat = m_DepthFormat,
		};

		VkGraphicsPipelineCreateInfo pipelineInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderInfo,
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertInputInfo,
			.pInputAssemblyState = &inputAssemblyInfo,
			.pViewportState = &viewportInfo,
			.pRasterizationState = &rasterInfo,
			.pMultisampleState = &multisampleInfo,
			.pDepthStencilState = &depthStencilInfo,
			.pColorBlendState = &blendInfo,
			.pDynamicState = &dynamicStateInfo,
			.layout = m_PipelineLayout,
			.renderPass = VK_NULL_HANDLE
		};

		VkPipeline pipeline = VK_NULL_HANDLE;

		VkResult result = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

		// shader modules aren't needed once the pipeline exists
		vkDestroyShaderModule(m_Device, vertexModule, nullptr);
		vkDestroyShaderModule(m_Device, fragmentModule, nullptr);

		VK_CHECK(result);

		m_GraphicsPipeline = pipeline;
		return pipeline;
	}

	bool VulkanContext::CreateSyncResources()
	{
		// timeline semaphore tracks when each frame is completed
		VkSemaphoreTypeCreateInfo semaphoreTypeInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = MaxFramesInFlight
		};
		VkSemaphoreCreateInfo semaphoreInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &semaphoreTypeInfo
		};

		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_TimelineSemaphore));

		// per-frame image-acquire semaphores (binary)
		for (FrameResources &res : m_FrameResources)
		{
			VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &res.imageAcquiredSemaphore));
		}

		return true;
	}

	bool VulkanContext::CreateCommandBuffers()
	{
		for (FrameResources &res : m_FrameResources)
		{
			// each frame gets its own pool, faster cmd buffer resets this way
			VkCommandPoolCreateInfo poolInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.queueFamilyIndex = m_GraphicsQueueFamilyIndex,
			};
			VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &res.commandPool));

			VkCommandBufferAllocateInfo cmdAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = res.commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &res.commandBuffer));
		}
		return true;
	}

	void VulkanContext::Render(const float* mvp)
	{
		// first check if our swapchain is still valid
		if (m_RequireSwapchainRecreate)
		{
			vkDeviceWaitIdle(m_Device);
			DestroySwapChain();
			CreateSwapChain(1600, 900);
			m_RequireSwapchainRecreate = false;
		}

		const uint32_t frameResIndex = frameIndex++ % MaxFramesInFlight;
		const uint64_t signalValue = nextSignalValue++;
		const uint64_t waitValue = signalValue - MaxFramesInFlight;

		// wait until the gpu is done with this frame's resources
		VkSemaphoreWaitInfo waitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores = &m_TimelineSemaphore,
			.pValues = &waitValue
		};
		vkWaitSemaphores(m_Device, &waitInfo, UINT64_MAX);

		// now its safe to start recording commands
		FrameResources &res = m_FrameResources[frameResIndex];
		vkResetCommandPool(m_Device, res.commandPool, 0);

		VkSemaphore imageAcquireSemaphore = m_FrameResources[frameResIndex].imageAcquiredSemaphore;

		uint32_t imageIndex = 0;
		VkResult acquireResult = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, imageAcquireSemaphore, VK_NULL_HANDLE, &imageIndex);

		// handle resize and out-of-date images, may need swapchain recreate
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_RequireSwapchainRecreate = true;
			return;
		}
		else if (acquireResult == VK_SUBOPTIMAL_KHR)
		{
			// can render this frame, recreate next time around
			m_RequireSwapchainRecreate = true;
		}

		VkCommandBufferBeginInfo cmdBeginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		vkBeginCommandBuffer(res.commandBuffer, &cmdBeginInfo);

		// transition the color and depth images
		std::vector<VkImageMemoryBarrier2> layoutBarriers
		{
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.image = m_SwapchainImages[imageIndex],
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				}
			},
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, // both specified to control memory access at both stages (write)
				.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.image = m_DepthImage,
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				}
			}
		};
		VkDependencyInfo depInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = static_cast<uint32_t>(layoutBarriers.size()),
			.pImageMemoryBarriers = layoutBarriers.data()
		};
		vkCmdPipelineBarrier2(res.commandBuffer, &depInfo);

		VkRenderingAttachmentInfo colorAttachInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_SwapchainImageViews[imageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // clear the image
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE, // keep data for presentation
			.clearValue{.color{0.01f, 0.01f, 0.01f, 1}}
		};
		VkRenderingAttachmentInfo depthAttachInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_DepthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // clear the depth data
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // don't care after rendering
			.clearValue{.depthStencil{1.0f, 0}}
		};
		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea
			{
				.offset{.x = 0, .y = 0},
				.extent{.width = m_SwapchainWidth, .height = m_SwapchainHeight}
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachInfo,
			.pDepthAttachment = &depthAttachInfo
		};

		vkCmdBeginRendering(res.commandBuffer, &renderingInfo);
		{
			VkViewport viewport
			{
				.x = 0,
				.y = 0,
				.width = static_cast<float>(m_SwapchainWidth),
				.height = static_cast<float>(m_SwapchainHeight),
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(res.commandBuffer, 0, 1, &viewport);

			VkRect2D scissor
			{
				.offset{.x = 0, .y = 0 },
				.extent{.width = m_SwapchainWidth, .height = m_SwapchainHeight}
			};
			vkCmdSetScissor(res.commandBuffer, 0, 1, &scissor);

			// draw our triangle
			vkCmdBindPipeline(res.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

			vkCmdPushConstants(res.commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, mvp);

			vkCmdDraw(res.commandBuffer, 3, 1, 0, 0);
		}
		vkCmdEndRendering(res.commandBuffer);

		// transition the image from color attachment to presentation so we can show it
		VkImageMemoryBarrier2 presentLayoutBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_NONE, // nothing is waiting, but the cache is flushed and layout is transition
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = m_SwapchainImages[imageIndex],
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		VkDependencyInfo presentDepInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &presentLayoutBarrier
		};
		vkCmdPipelineBarrier2(res.commandBuffer, &presentDepInfo);

		vkEndCommandBuffer(res.commandBuffer);

		VkSemaphoreSubmitInfo imageAcquireWaitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = imageAcquireSemaphore,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT // wait before drawing to image
		};
		std::vector<VkSemaphoreSubmitInfo> semaphoreSignals
		{
			{ // render work completion signal
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = m_RenderCompleteSemaphores[imageIndex],
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			},
			{ // entire frame is completed (timeline)
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = m_TimelineSemaphore,
				.value = signalValue,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			}
		};
		VkCommandBufferSubmitInfo cmdSubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = res.commandBuffer,
		};
		VkSubmitInfo2 submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &imageAcquireWaitInfo, // ensure the image is ready
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmdSubmitInfo,
			.signalSemaphoreInfoCount = static_cast<uint32_t>(semaphoreSignals.size()),
			.pSignalSemaphoreInfos = semaphoreSignals.data()
		};
		vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

		VkPresentInfoKHR presentInfo
		{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_RenderCompleteSemaphores[imageIndex], // render work completed semaphore
			.swapchainCount = 1,
			.pSwapchains = &m_Swapchain,
			.pImageIndices = &imageIndex,
			.pResults = nullptr
		};

		vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
	}
}