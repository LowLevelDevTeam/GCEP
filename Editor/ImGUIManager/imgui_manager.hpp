#pragma once

/// @file imgui_manager.hpp
/// @brief Thin RAII wrapper around the Dear ImGui Vulkan/GLFW backends.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Externals
#include <GLFW/glfw3.h>
#include <imgui_impl_vulkan.h>

namespace gcep::UI
{
    /// @class ImGuiManager
    /// @brief RAII manager for the Dear ImGui context and its Vulkan + GLFW backends.
    ///
    /// @c ImGuiManager centralises the three-step ImGui lifecycle —
    /// initialisation, per-frame begin/end, and shutdown — so that the rest of the
    /// editor code only calls the high-level helpers and never touches the raw
    /// backend functions directly.
    ///
    /// @par Typical usage
    /// @code
    /// UI::ImGuiManager imguiManager;
    /// imguiManager.init(glfwWindow, vulkanInitInfo);
    ///
    /// while (running)
    /// {
    ///     imguiManager.beginFrame();
    ///     // ... ImGui calls ...
    ///     imguiManager.endFrame();
    /// }
    ///
    /// imguiManager.shutdown();
    /// @endcode
    ///
    /// @note All methods must be called from the main/render thread.
    class ImGuiManager
    {
    public:
        ImGuiManager();

        /// @brief Initialises the Dear ImGui context and both platform/renderer backends.
        ///
        /// Steps performed:
        /// -# Creates the ImGui context and configures the @c ImGuiIO flags
        ///    (docking, viewports).
        /// -# Calls @c ImGui_ImplGlfw_InitForVulkan() with @p window.
        /// -# Calls @c ImGui_ImplVulkan_Init() with @p initInfo.
        /// -# Uploads the font atlas to the GPU.
        ///
        /// @param window    GLFW window to bind the GLFW backend to. Must outlive this object.
        /// @param initInfo  Vulkan backend initialisation parameters (instance, device, queue, etc.).
        void init(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);

        /// @brief Shuts down both backends and destroys the ImGui context.
        ///
        /// Calls @c ImGui_ImplVulkan_Shutdown(), @c ImGui_ImplGlfw_Shutdown(), and
        /// @c ImGui::DestroyContext(). Safe to call even if @c init() was never completed.
        void shutdown();

        /// @brief Begins a new Dear ImGui frame.
        ///
        /// Calls @c ImGui_ImplVulkan_NewFrame(), @c ImGui_ImplGlfw_NewFrame(), and
        /// @c ImGui::NewFrame() in that order. Must be called once per render loop
        /// iteration before any @c ImGui::* draw calls.
        void beginFrame();

        /// @brief Ends the current Dear ImGui frame and submits draw data.
        ///
        /// Calls @c ImGui::Render(). If multi-viewport is enabled, also calls
        /// @c ImGui::UpdatePlatformWindows() and @c ImGui::RenderPlatformWindowsDefault().
        void endFrame();

        /// @brief Returns whether @c init() completed successfully.
        /// @returns @c true if the manager is ready to render frames.
        [[nodiscard]] bool isInitialized() const { return m_initialized; }

    private:
        /// @brief Tracks whether the ImGui backends have been successfully initialised.
        bool m_initialized;
    };
} // namespace gcep::UI
