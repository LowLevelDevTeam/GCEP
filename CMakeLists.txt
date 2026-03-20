cmake_minimum_required(VERSION 3.30)

project(GCEngine VERSION 0.1 LANGUAGES CXX)

# Language standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Warnings disable
set(INTERPROCEDURAL_OPTIMIZATION OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Custom CMake modules
include(CMake/compile_shaders.cmake)
include(CMake/copy_textures.cmake)

# Vulkan
find_package(Vulkan 1.3 REQUIRED)

# Sources
file(GLOB_RECURSE CPP_SOURCES
    Editor/*.cpp Editor/*.inl
    Engine/*.cpp Engine/*.inl
)
file(GLOB_RECURSE ENGINE_SOURCE
    Engine/*.cpp Engine/*.inl
)

# Main executable
add_executable(GCEngine ${CPP_SOURCES})

target_include_directories(GCEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}             # <Editor/...> <Engine/...>
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Core # <Log/log.hpp> etc.
    ${GCE_SCRIPT_CONFIG_OUT}
)

target_link_libraries(GCEngine PRIVATE
    Vulkan::Vulkan
    GCExternals
)

# Hide console window on Windows Release builds
if(WIN32 AND CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
    if(MSVC)
        target_link_options(GCEngine PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    else()
        target_link_options(GCEngine PRIVATE -mwindows)
    endif()
endif()

# Shared externals interface library
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Externals/boost_pfr/include
    ${GCE_SCRIPT_CONFIG_OUT}
    ${Vulkan_INCLUDE_DIRS}
)

# Subdirectories
add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Externals)
# add_subdirectory(Scripts)    # Pending
# add_subdirectory(UnitTests)  # TODO: uncomment when ready

# Engine static lib (for unit tests)
add_library(GCEngineLib STATIC ${ENGINE_SOURCE})
target_link_libraries(GCEngineLib PRIVATE GCExternals)

# Assets
file(GLOB_RECURSE SHADER_SOURCES Assets/*.slang)
add_slang_shader_target(Shaders SOURCES ${SHADER_SOURCES})
copy_textures(Textures)

# Global defines
target_compile_definitions(GCEngine PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    GCE_BUILD_DIR="${CMAKE_BINARY_DIR}"
    NOMINMAX
)

add_dependencies(GCEngine Shaders Textures)
