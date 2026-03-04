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
# set(LIBKTX_VERSION_FULL ON)
# add_subdirectory(KTX-Software/lib)
# target_include_directories(GCEngine PRIVATE KTX-Software/lib/include)
# add_lib_dependencies(GCEngine ktx)

# stb image
target_include_directories(GCEngine PRIVATE stb)

# Final libraries linking
target_link_libraries(GCEngine PRIVATE ImGui ImGuizmo glfw miniaudio Jolt)