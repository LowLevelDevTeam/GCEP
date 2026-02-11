# GLFW
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

# KTX-Software
set(LIBKTX_VERSION_FULL       ON)
set(LIBKTX_FEATURE_KTX1       ON)
set(LIBKTX_FEATURE_KTX2       ON)
set(LIBKTX_FEATURE_VK_UPLOAD  ON)
set(LIBKTX_FEATURE_GL_UPLOAD  ON)
set(LIBKTX_FEATURE_ETC_UNPACK ON)
add_subdirectory(KTX-Software/lib)
target_include_directories(GCEngine PRIVATE KTX-Software/lib/include)
add_lib_dependencies(GCEngine ktx)

# Final libraries linking
target_link_libraries(GCEngine PRIVATE ImGui ImGuizmo glfw miniaudio Jolt ktx)