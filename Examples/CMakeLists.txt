# ==== Example Dependencies ====

include(FetchContent)

FetchContent_Declare(imgui
		GIT_REPOSITORY https://github.com/ocornut/imgui.git
		GIT_TAG v1.93.0-docking
		GIT_SHALLOW TRUE
		SYSTEM
		EXCLUDE_FROM_ALL
)

FetchContent_Declare(glm
		GIT_REPOSITORY https://github.com/g-truc/glm.git
		GIT_TAG 1.0.2
		GIT_SHALLOW TRUE
		SYSTEM
		EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(imgui glm)

# ==== ImGui ====

add_library(SlateImGui STATIC
		"${imgui_SOURCE_DIR}/imgui.cpp"
		"${imgui_SOURCE_DIR}/imgui_draw.cpp"
		"${imgui_SOURCE_DIR}/imgui_tables.cpp"
		"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
)

target_include_directories(SlateImGui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")

target_link_libraries(SlateImGui PUBLIC SDL3::SDL3)

# ==== Example Application ====

add_library(SlateApp STATIC
		SlateApp.cpp
		SlateApp.hpp
)

target_include_directories(SlateApp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(SlateApp
		PUBLIC
		Slate::Slate
		SDL3::SDL3
		SlateImGui
)

# ==== Runtime Dependencies ====

function(slate_copy_runtime_files target)
	if(NOT WIN32)
		return()
	endif()

	add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:${target}>
			"$<TARGET_FILE_DIR:${target}>"
			COMMAND_EXPAND_LISTS
			VERBATIM
	)

	if(DEFINED SLATE_SLANG_RUNTIME_DIR
			AND IS_DIRECTORY "${SLATE_SLANG_RUNTIME_DIR}")

		add_custom_command(TARGET ${target} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${SLATE_SLANG_RUNTIME_DIR}"
				"$<TARGET_FILE_DIR:${target}>"
				VERBATIM
		)
	endif()
endfunction()

# ==== Examples ====

function(slate_add_example name)
	add_executable(${name} "${name}.cpp")
	target_link_libraries(${name} PRIVATE SlateApp glm::glm)
	set_target_properties(${name} PROPERTIES FOLDER "Examples")
	slate_copy_runtime_files(${name})
endfunction()

# Add examples here as the library develops.
# slate_add_example(001_ClearScreen)
slate_add_example(000_Window)
slate_add_example(001_HelloTriangle)
slate_add_example(002_Cube)