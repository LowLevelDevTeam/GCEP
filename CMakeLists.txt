cmake_minimum_required(VERSION 3.30)

project(GCEngine VERSION 0.1 LANGUAGES CXX)

include(CMake/compile_shaders.cmake)
include(CMake/copy_textures.cmake)

# Generated build config for scripting
set(GCE_SCRIPT_CONFIG_OUT "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GCE_SCRIPT_CONFIG_OUT}")
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Core/Scripting/ScriptBuildConfig.hpp.in
        ${GCE_SCRIPT_CONFIG_OUT}/ScriptBuildConfig.hpp
        @ONLY
)

# Set properties
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build set to release
if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "." FORCE)
endif()

file(GLOB_RECURSE CPP_SOURCES Editor/*.cpp Engine/*.cpp Editor/*.inl Engine/*.inl)
file(GLOB_RECURSE ENGINE_SOURCE Engine/*.cpp Engine/*.inl)

# Adding the executable
add_executable(GCEngine Engine/Core/Scripting/ScriptModel.cpp)
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

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Externals)
#if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/UnitTests)
#    add_subdirectory(UnitTests)
#endif()



add_library(GCExternals INTERFACE)
target_include_directories(GCExternals INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/glm
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/JoltPhysics
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/KTX-Software/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/KTX-Software/external/dfdutils
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/miniaudio
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/glfw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ImGuizmo
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/TinyGLTF
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/Externals/FontAwesome
        ${GCE_SCRIPT_CONFIG_OUT}
        ${Vulkan_INCLUDE_DIRS}
)

# Scripting support (lightweight lib for tools)
add_library(GCEngineScripting STATIC
        Engine/Core/Scripting/SharedLibrary.cpp
        Engine/Core/Scripting/ScriptHost.cpp
)

add_definitions(-DPROJECT_ROOT="${CMAKE_SOURCE_DIR}")

target_link_libraries(GCEngineScripting PUBLIC GCExternals)

# Scripts
add_subdirectory(Scripts)

# Compile the engine into a lib (to use GoogleTest)
add_library(GCEngineLib STATIC ${ENGINE_SOURCE})
target_link_libraries(GCEngineLib PRIVATE GCExternals)

# Add top level directories as include paths e.g. <Editor/...> or <Engine/...>
target_include_directories(GCEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# e.g. <RHI/RHI.hpp>
target_include_directories(GCEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Core)
# Generated scripting config
target_include_directories(GCEngine PRIVATE ${GCE_SCRIPT_CONFIG_OUT})

# Compile the shaders
file(GLOB_RECURSE SHADER_SOURCES Assets/*.slang)
add_slang_shader_target(Shaders SOURCES ${SHADER_SOURCES})

# Copy the test textures
copy_textures(Textures)

add_dependencies(GCEngine Shaders Textures)
add_dependencies(GCEngine GCEngineScripts)
add_dependencies(GCEngine GCEngineScripts)