cmake_minimum_required(VERSION 3.30)

project(GCEngine VERSION 0.1 LANGUAGES CXX)

# Set properties
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build set to release
if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "." FORCE)
endif()

file(GLOB_RECURSE CPP_SOURCES Editor/*.cpp Engine/*.cpp)
message(STATUS ${CPP_SOURCES})

# Adding the executable
add_executable(GCEngine)
target_sources(GCEngine PRIVATE ${CPP_SOURCES})

# Vulkan
find_package(Vulkan 1.3 REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found")
endif()

# Linker options (to disable console window on Windows in Release build)
if(WIN32 AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    if(MSVC)
        target_link_options(GCEngine PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    else()
        target_link_options(GCEngine PRIVATE -mwindows)
    endif()
endif()

target_include_directories(GCEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(GCEngine PRIVATE Vulkan::Vulkan)

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Externals)