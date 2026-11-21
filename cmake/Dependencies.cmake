include_guard(GLOBAL)
include(FetchContent)

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "SlateVK supports Windows and Linux only")
endif()

# Vulkan dependencies are bundled with the repository.
# Vulkan-Headers provides headers; volk loads the Vulkan API; VMA manages memory.
set(VULKAN_HEADERS_ENABLE_TESTS OFF)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF)
set(VULKAN_HEADERS_ENABLE_MODULE OFF)
set(VOLK_PULL_IN_VULKAN OFF)
set(VOLK_INSTALL OFF)
set(VMA_ENABLE_INSTALL OFF)
set(VUL_ENABLE_INSTALL OFF)

if(NOT TARGET Vulkan::Headers)
	add_subdirectory("${PROJECT_SOURCE_DIR}/External/Vulkan-Headers" "${PROJECT_BINARY_DIR}/_deps/vulkan-headers" EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET Vulkan::UtilityHeaders)
	add_subdirectory("${PROJECT_SOURCE_DIR}/External/Vulkan-Utility-Libraries" "${PROJECT_BINARY_DIR}/_deps/vulkan-utility-libraries" EXCLUDE_FROM_ALL)
endif()

# Slate defines VOLK_IMPLEMENTATION itself, so use volk's header-only target.
if(NOT TARGET volk_headers)
	add_subdirectory("${PROJECT_SOURCE_DIR}/External/volk" "${PROJECT_BINARY_DIR}/_deps/volk" EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET GPUOpen::VulkanMemoryAllocator)
	add_subdirectory("${PROJECT_SOURCE_DIR}/External/VulkanMemoryAllocator" "${PROJECT_BINARY_DIR}/_deps/vma" EXCLUDE_FROM_ALL)
endif()

# SDL3 is the sole window-system dependency of SlateVK.
# Keep its tests and examples out of the consumer's build.
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_EXAMPLES OFF)

if(NOT TARGET SDL3::SDL3)
	FetchContent_Declare(SDL3
			GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
			GIT_TAG release-3.4.18
			GIT_SHALLOW TRUE
			SYSTEM
			EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(SDL3)
endif()

# SlateVK currently compiles shaders through Slang, so this is a core dependency.
set(SLATE_SLANG_VERSION "2026.20")

if(NOT TARGET slang::slang)
	if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|x64|X64)$")
		message(FATAL_ERROR "Prebuilt Slang currently requires an x86-64 target")
	endif()

	if(WIN32)
		set(_slate_platform windows)
		set(_slate_archive zip)
	else()
		set(_slate_platform linux)
		set(_slate_archive tar.gz)
	endif()

	FetchContent_Declare(SlateSlang
			URL "https://github.com/shader-slang/slang/releases/download/v${SLATE_SLANG_VERSION}/slang-${SLATE_SLANG_VERSION}-${_slate_platform}-x86_64.${_slate_archive}"
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
			SOURCE_SUBDIR "_prebuilt_no_cmake_project"
	)
	FetchContent_MakeAvailable(SlateSlang)

	find_package(slang CONFIG REQUIRED
			PATHS "${slateslang_SOURCE_DIR}" "${slateslang_SOURCE_DIR}/cmake"
			NO_DEFAULT_PATH
	)
	set(SLATE_SLANG_RUNTIME_DIR "${slateslang_SOURCE_DIR}/bin" CACHE INTERNAL "Slang runtime directory")
endif()

# Optional profiling, disabled for consumers by default.
if(SLATE_ENABLE_TRACY AND NOT TARGET Tracy::TracyClient)
	set(TRACY_ENABLE ON)
	set(TRACY_ON_DEMAND ON)
	FetchContent_Declare(Tracy
			GIT_REPOSITORY https://github.com/wolfpld/tracy.git
			GIT_TAG v0.14.1
			GIT_SHALLOW TRUE
			SYSTEM
			EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(Tracy)
endif()