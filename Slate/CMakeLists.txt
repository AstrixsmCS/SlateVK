add_library(Slate STATIC
		Slate.cpp
		Slate.hpp
		Vulkan/Context.cpp
		Vulkan/Context.hpp
)

add_library(Slate::Slate ALIAS Slate)

target_compile_features(Slate PUBLIC cxx_std_23)

target_include_directories(Slate PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)

target_link_libraries(Slate
		PUBLIC
		Vulkan::Headers
		volk_headers
		PRIVATE
		GPUOpen::VulkanMemoryAllocator
		SDL3::SDL3
		slang::slang
)

# Do not also link volk's compiled library or compile VMA separately.
target_compile_definitions(Slate PRIVATE
		VK_NO_PROTOTYPES
		VMA_STATIC_VULKAN_FUNCTIONS=0
		VMA_DYNAMIC_VULKAN_FUNCTIONS=1

		NOMINMAX
		WIN32_LEAN_AND_MEAN
)

if(SLATE_ENABLE_TRACY)
	target_link_libraries(Slate PRIVATE Tracy::TracyClient)
	target_compile_definitions(Slate PUBLIC SLATE_ENABLE_TRACY)
endif()

set_target_properties(Slate PROPERTIES
		CXX_EXTENSIONS OFF
		POSITION_INDEPENDENT_CODE ON
)
