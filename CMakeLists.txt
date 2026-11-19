cmake_minimum_required(VERSION 3.30)

project(SlateVK VERSION 0.1.0 LANGUAGES C CXX)

option(SLATE_BUILD_EXAMPLES "Build SlateVK examples" ${PROJECT_IS_TOP_LEVEL})
option(SLATE_ENABLE_TRACY "Enable Tracy profiling" OFF)

# Resolve shared dependencies once, before creating targets.
include(cmake/Dependencies.cmake)

add_subdirectory(Slate)

if(SLATE_BUILD_EXAMPLES)
	add_subdirectory(Examples)
endif()
