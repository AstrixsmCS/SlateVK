#pragma once

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <cassert>
#include <cstdlib>
#include <print>
#include <source_location>

namespace Slate
{
	inline void CheckVkResult(VkResult result, std::source_location location = std::source_location::current())
	{
		if (result == VK_SUCCESS)
			return;

		std::println(stderr, "Vulkan error: {} ({}:{})", string_VkResult(result), location.file_name(), location.line());

		assert(false && "VK_CHECK failed");
		std::abort();
	}
}

#define VK_CHECK(expression) \
	::Slate::CheckVkResult((expression))