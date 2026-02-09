#pragma once
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <vulkan/vulkan_raii.hpp>

namespace gcep
{
class UiManager {
public:

    // @brief creation of the Editoreditor UI
    UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);

    void uiUpdate();

    void beginRender();

    void render(vk::raii::CommandBuffer commandBuffer);

    void endRender();

private:

    GLFWwindow* m_window;
    ImGui_ImplVulkan_InitInfo m_initInfo;
    bool showDemoWindow = true;
};

}
