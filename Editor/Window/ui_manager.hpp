#pragma once
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <vulkan/vulkan_raii.hpp>
#include <functional>
#include <glm/glm.hpp>

namespace gcep
{
class Camera;
class UiManager {
public:

    // @brief creation of the Editoreditor UI
    UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);

    /// @brief Update UI with viewport texture
    /// @param sceneTexture The descriptor set for the offscreen rendered scene
    /// @param viewportResizeCallback Callback called when viewport size changes
    void uiUpdate(VkDescriptorSet sceneTexture, const std::function<void(uint32_t, uint32_t)>& viewportResizeCallback, Camera* camera, uint32_t drawCount);

    ImVec4& getClearColor();

    /// @brief Get the current viewport size
    [[nodiscard]] ImVec2 getViewportSize() const { return m_viewportSize; }
    [[nodiscard]] glm::vec3 getAmbientColor() const { return ambientColor; }
    [[nodiscard]] glm::vec3 getLightColor() const { return lightColor; }
    [[nodiscard]] glm::vec3 getLightDirection() const { return lightDirection; }

    float f = 2.0f; // Camera speed, exposed to UI for real-time tweaking

private:

    GLFWwindow* m_window;
    ImGui_ImplVulkan_InitInfo m_initInfo;
    bool showDemoWindow = false;
    ImVec2 m_viewportSize = {800, 600};
    ImVec4 m_clearColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    glm::vec3 ambientColor = {0.2f, 0.2f, 0.2f};
    glm::vec3 lightColor = {0.5f, 0.5f, 0.5f};
    glm::vec3 lightDirection = {1.0f, 0.3f, 0.3f};
};

}
