#pragma once
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "TextEditor.h"
#include "imgui_impl_vulkan.h"
#include <vulkan/vulkan_raii.hpp>
#include <functional>

namespace gcep
{
class UiManager {
public:

    void initEditor();
    // @brief creation of the Editoreditor UI
    UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);

    ~UiManager();
    void uiUpdate();

    /// @brief Update UI with viewport texture
    /// @param sceneTexture The descriptor set for the offscreen rendered scene
    /// @param viewportResizeCallback Callback called when viewport size changes
    void uiUpdate(VkDescriptorSet sceneTexture, const std::function<void(uint32_t, uint32_t)>& viewportResizeCallback);

    ImVec4& getClearColor();

    /// @brief Get the current viewport size
    [[nodiscard]] ImVec2 getViewportSize() const { return m_viewportSize; }

    float f = 2.0f; // Camera speed, exposed to UI for real-time tweaking

private:

    TextEditor editor;
    GLFWwindow* m_window;
    ImGui_ImplVulkan_InitInfo m_initInfo;
    bool showDemoWindow = true;
    ImVec2 m_viewportSize = {800, 600};
    ImVec4 m_clearColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
};

}
