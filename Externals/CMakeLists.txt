# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_OSMESA OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)
target_include_directories(GCEngine PRIVATE glfw/include)

# GLM
add_subdirectory(glm)
target_include_directories(GCEngine PRIVATE glm)

# ImGui
add_subdirectory(imgui)
target_include_directories(GCEngine PRIVATE imgui)

# ImGuizmo
add_subdirectory(ImGuizmo)
target_include_directories(GCEngine PRIVATE ImGuizmo)

# JoltPhysics
add_subdirectory(JoltPhysics/Build)
target_include_directories(GCEngine PRIVATE JoltPhysics)

# Miniaudio
add_subdirectory(miniaudio)
target_include_directories(GCEngine PRIVATE miniaudio)

# TinyFileDialogs
target_include_directories(GCEngine PRIVATE TinyFileDialogs)
target_sources(GCEngine PRIVATE TinyFileDialogs/tinyfiledialogs.cpp)

# Font Awesome
target_include_directories(GCEngine PRIVATE FontAwesome)

# KTX-Software
set(KTX_VERSION 4.4.2)
set(KTX_GIT_VERSION_FULL 4.4.2)
set(LIBKTX_VERSION_FULL ON)
add_subdirectory(KTX-Software/lib)
target_include_directories(GCEngine PRIVATE KTX-Software/lib/include)
target_include_directories(GCEngine PRIVATE KTX-Software/external/dfutils)
add_lib_dependencies(GCEngine ktx)

# TinyGLTF
target_include_directories(GCEngine PRIVATE TinyGLTF)

# stb image
target_include_directories(GCEngine PRIVATE stb)

# boost_pfr
target_include_directories(GCEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/boost_pfr/include)

# Final libraries linking
target_link_libraries(GCEngine PRIVATE ImGui ImGuizmo glfw miniaudio Jolt ktx)