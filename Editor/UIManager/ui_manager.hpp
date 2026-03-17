#pragma once

/// @file ui_manager.hpp
/// @brief Top-level editor UI orchestrator: dockspace, menu bar, panel lifecycle.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 2.0
/// @date 2026-03-16

// Internals
#include <Editor/ProjectLoader/project_loader.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/ViewPort/viewport_panel.hpp>
#include <Editor/UI_Panels/Panels/SceneSettings/scene_settings_panel.hpp>
#include <Editor/UI_Panels/Panels/SceneHierarchy/scene_hierarchy_panel.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_panel.hpp>
#include <Editor/UI_Panels/Panels/AudioControl/audio_control_panel.hpp>
#include <Editor/UI_Panels/Panels/Console/console_panel.hpp>
#include <Editor/UI_Panels/Panels/ContentBrowser/content_browser.hpp>
#include <Editor/UI_Panels/Panels/ProjectBrowser/project_browser_panel.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>
#include <Scene/header/scene_manager.hpp>

// Externals
#include <GLFW/glfw3.h>
#include <imgui.h>

// STL
#include <string>

namespace gcep
{
    class Camera;

    // ─────────────────────────────────────────────────────────────────────────────

    /// @class UiManager
    /// @brief Orchestrates the editor UI frame: dockspace layout, menu bar, all panels.
    ///
    /// @c UiManager owns one instance of each @c IPanel subclass.
    /// It delegates all panel drawing to those instances and uses the
    /// @c EditorContext singleton as the shared communication bus between panels.
    ///
    /// @par Frame sequence (uiUpdate)
    /// @code
    /// beginFrame()                   // ImGuizmo::BeginFrame
    /// setupViewport + getDockspaceID + initDockspace (once)
    /// drawMainMenuBar()
    /// m_viewport.draw()
    /// m_settings.draw()              // shown via menu toggle
    /// m_hierarchy.draw()
    /// m_inspector.draw()
    /// m_audio.draw()
    /// m_console.draw()
    /// m_contentBrowser.draw()
    /// drawBottomBar()
    /// updateSceneUBO / updateCameraUBO
    /// @endcode
    class UiManager
    {
    public:
        /// @brief Constructs the manager and wires the scene manager into EditorContext.
        ///
        /// @param window    GLFW window used for input and viewport sizing.
        /// @param manager   Active scene manager.
        /// @param reload    Set to @c true when a project reload is requested.
        /// @param close     Set to @c false when the user requests exit.
        UiManager(GLFWwindow* window, SLS::SceneManager* manager, bool& reload, bool& close);

        ~UiManager() = default;

        /// @brief Binds the live RHI data to EditorContext and the viewport panel.
        void setInfos(rhi::vulkan::InitInfos* initInfos);

        /// @brief Stores the editor camera into EditorContext.
        void setCamera(Camera* pCamera);

        /// @brief Updates the scene manager and registry references in EditorContext.
        void setSceneManager(SLS::SceneManager* sceneManager);

        /// @brief No-op kept for API compatibility; ProjectLoader is a singleton.
        void setProjectLoader(pl::ProjectLoader*) {}

        /// @brief Executes one full editor frame.
        void uiUpdate();

        /// @brief Propagates project-file settings into EditorContext and the RHI.
        void applyLoadedSettings();

    private:
        // ── Dockspace helpers ─────────────────────────────────────────────────────

        static ImGuiViewport* setupViewport();
        ImGuiID        getDockspaceID();
        void           initDockspace(const ImGuiID& dockspaceId, const ImGuiViewport* viewport);
        void           drawMainMenuBar();
        void           drawBottomBar();
        void           beginFrame();

        // ── Sync helper ───────────────────────────────────────────────────────────

        /// @brief Copies ECS transform/physics back into RHI mesh data every frame.
        void syncECSToRHI();

        // ── State ─────────────────────────────────────────────────────────────────

        GLFWwindow*        m_window;
        bool&              m_reloadApp;
        bool&              m_isRunning;
        std::string        m_currentScenePath;
        SLS::SceneManager* m_sceneManager = nullptr;
        bool               m_showDemoWindow = false;
        bool               m_showSettings   = false;

        // ── Panels ────────────────────────────────────────────────────────────────

        panel::ViewportPanel       m_viewport;
        panel::SceneSettingsPanel  m_settings;
        panel::SceneHierarchyPanel m_hierarchy;
        panel::InspectorPanel      m_inspector;
        panel::AudioControlPanel    m_audio;
        panel::ConsolePanel         m_console;
        editor::ContentBrowser      m_contentBrowser;
        panel::ProjectBrowserPanel  m_projectBrowser;
    };

} // namespace gcep
