#pragma once

// Externals
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

namespace gcep::UI {
    class ImGUIManager
    {
    public:
        ImGUIManager();

        void init(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);
        void shutdown();
        void beginFrame();
        void endFrame();

        [[nodiscard]] bool isInitialized() const { return m_initialized; }
    private:
        bool m_initialized;
    };
} // Namespace gcep::UI
