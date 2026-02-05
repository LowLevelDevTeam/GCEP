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
target_include_directories(GCEngine PRIVATE JoltPhysics)

# Miniaudio
add_subdirectory(miniaudio)
target_include_directories(GCEngine PRIVATE miniaudio)

# Final libraries linking
target_link_libraries(GCEngine PRIVATE ImGui ImGuizmo glfw miniaudio)