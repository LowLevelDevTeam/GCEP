#pragma once

/// @file ui_manager.hpp
/// @brief Top-level editor UI orchestrator: dockspace, menu bar, panel lifecycle.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 2.0
/// @date 2026-03-16

// Internals
#include <Editor/ProjectLoader/project_loader.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/AudioControl/audio_control_panel.hpp>
#include <Editor/UI_Panels/Panels/Console/console_panel.hpp>
#include <Editor/UI_Panels/Panels/ContentBrowser/content_browser.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_panel.hpp>
#include <Editor/UI_Panels/Panels/Performance/performance_panel.hpp>
#include <Editor/UI_Panels/Panels/SceneHierarchy/scene_hierarchy_panel.hpp>
#include <Editor/UI_Panels/Panels/SceneSettings/scene_settings_panel.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_manager_panel.hpp>
#include <Editor/UI_Panels/Panels/ViewPort/viewport_panel.hpp>
#include <Engine/Core/Scripting/script_hot_reload.hpp>
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
    /// @c UiManager owns one instance of each @c IPanel subclass and is responsible
    /// for driving their per-frame @c draw() calls in the correct order. It uses
    /// @c EditorContext as the shared communication bus between panels, and propagates
    /// external pointers (RHI, camera, scene manager) into that context.
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
    /// syncECSToRHI()
    /// @endcode
    ///
    /// @par Dockspace initialisation
    /// The dockspace layout is built exactly once (when @c m_dockspaceInitialized is
    /// @c false) by @c initDockspace(). Subsequent frames reuse the persisted layout.
    ///
    /// @note All methods must be called from the main thread.
    class UiManager
    {
    public:
        /// @brief Constructs the manager and wires the scene manager into @c EditorContext.
        ///
        /// Stores references to the caller-owned @p reload and @p close flags so
        /// that the menu bar and toolbar can trigger application-level state changes
        /// without coupling to @c Application directly.
        ///
        /// @param window    GLFW window used for input queries and viewport sizing.
        /// @param manager   Active scene manager forwarded to @c EditorContext.
        /// @param reload    Flag set to @c true when a project reload is requested.
        /// @param close     Flag set to @c false when the user requests application exit.
        UiManager(GLFWwindow* window, SLS::SceneManager* manager, bool& reload, bool& close);

        ~UiManager() = default;

        /// @brief Binds the live RHI data to @c EditorContext and the viewport panel.
        ///
        /// Forwards @p initInfos to @c EditorContext::pRHI and calls
        /// @c ViewportPanel::setViewportTexture() with the descriptor set found inside.
        ///
        /// @param initInfos  Non-owning pointer to the RHI initialisation info struct.
        void setInfos(rhi::vulkan::InitInfos* initInfos);

        /// @brief Stores the editor camera pointer into @c EditorContext.
        ///
        /// @param pCamera  Non-owning pointer to the active editor @c Camera.
        void setCamera(Camera* pCamera);

        /// @brief No-op kept for API compatibility; @c ProjectLoader is a singleton.
        ///
        /// @param  Unused project loader pointer.
        void setProjectLoader(pl::ProjectLoader*) {}

        /// @brief Executes one full editor frame.
        ///
        /// Must be called once per application frame, after @c ImGui::NewFrame() and
        /// before @c ImGui::Render(). See the class-level frame-sequence diagram for
        /// the call order.
        void uiUpdate();

        /// @brief Propagates project-file settings into @c EditorContext and the RHI.
        ///
        /// Called after a project or scene is loaded to apply persisted configuration
        /// (render settings, scene UBO values, etc.) to the running subsystems.
        void applyLoadedSettings();

    private:
        // ── Dockspace helpers ─────────────────────────────────────────────────────

        /// @brief Configures the full-screen ImGui viewport used as the dockspace host.
        ///
        /// @returns Pointer to the primary ImGui viewport.
        static ImGuiViewport* setupViewport();

        /// @brief Retrieves (or creates) the dockspace ID for the main window.
        ///
        /// @returns ImGui dockspace identifier.
        ImGuiID getDockspaceID();

        /// @brief Builds the initial panel layout inside the dockspace.
        ///
        /// Called only once (@c m_dockspaceInitialized is set to @c true afterwards).
        /// Splits the dockspace into the canonical editor regions and docks each panel.
        ///
        /// @param dockspaceId  ID of the root dockspace node to split.
        /// @param viewport     Viewport whose size is used for proportional splits.
        void initDockspace(const ImGuiID& dockspaceId, const ImGuiViewport* viewport);

        /// @brief Renders the top-level menu bar (File, Edit, View, …).
        ///
        /// Handles menu actions such as opening scenes, toggling panels, and
        /// requesting application reload or exit.
        void drawMainMenuBar();

        /// @brief Renders the status bar anchored to the bottom of the main window.
        ///
        /// Displays frame-rate, selected-entity info, and other lightweight metrics.
        void drawBottomBar();

        /// @brief Calls @c ImGuizmo::BeginFrame() to reset gizmo state at frame start.
        void beginFrame();

        // ── Sync helper ───────────────────────────────────────────────────────────

        /// @brief Copies ECS transform and physics state back into RHI mesh data every frame.
        ///
        /// Iterates all entities with both a @c Transform and an RHI mesh / light
        /// component, writing the current world matrix so the renderer always reflects
        /// the latest ECS state.
        void syncECSToRHI();

        // ── State ─────────────────────────────────────────────────────────────────

        /// @brief Non-owning pointer to the GLFW window used for input and sizing.
        GLFWwindow*        m_window;

        /// @brief Reference to the @c Application::m_reloadRequested flag.
        bool&              m_reloadApp;

        /// @brief Reference to the @c Application::m_isRunning flag.
        bool&              m_isRunning;

        /// @brief Non-owning pointer to the active scene manager.
        SLS::SceneManager* m_sceneManager        = nullptr;

        /// @brief True once @c initDockspace() has executed; prevents re-splitting.
        bool               m_dockspaceInitialized = false;

        /// @brief Controls visibility of the Dear ImGui demo window (debug aid).
        bool               m_showDemoWindow       = false;

        /// @brief Path of a scene scheduled to be loaded on the next frame.
        std::string        m_pendingScenePath;

        // ── Panels ────────────────────────────────────────────────────────────────

        editor::ContentBrowser      m_contentBrowser;      ///< Project asset browser.
        panel::ViewportPanel        m_viewport;            ///< 3D scene viewport.
        panel::SceneSettingsPanel   m_settings;            ///< Render / environment settings.
        panel::SceneHierarchyPanel  m_hierarchy;           ///< Entity tree view.
        panel::InspectorPanel       m_inspector;           ///< Component property editor.
        panel::AudioControlPanel    m_audio;               ///< Audio playback controls.
        panel::ConsolePanel         m_console;             ///< Log output panel.
        panel::PerformancePanel     m_performance;         ///< Frame-time and profiling panel.
        panel::ScriptState          m_scriptState;         ///< Hot-reload script state shared with panels.
        panel::ScriptManagerPanel   m_scriptManagerPanel;  ///< Script asset management panel.
    };

} // namespace gcep
