#pragma once

/// @file editor_context.hpp
/// @brief Singleton data bus shared across all editor panels.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.0
/// @date 2026-02-17

// Internals
#include <Engine/RHI/Vulkan/vulkan_rhi.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>
#include <ECS/headers/registry.hpp>
#include <Editor/Camera/camera.hpp>
#include <Editor/Helpers.hpp>
#include <Editor/UI_Panels/selection_manager.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_manager_panel.hpp>
#include <Scripting/script_hot_reload.hpp>

// Externals
#include <imgui.h>
#include <ImGuizmo.h>

// STL
#include <filesystem>
#include <vector>

namespace gcep::editor
{
    /// @struct EditorContext
    /// @brief Singleton that acts as the shared communication bus between all editor panels.
    ///
    /// @c EditorContext centralises every piece of mutable state that multiple editor
    /// panels need to read or write without direct coupling to one another.  All panels
    /// obtain a reference through @c EditorContext::get() and operate on the same
    /// instance for the entire lifetime of the editor process.
    ///
    /// @par Categories of state
    /// - **Core** – pointers to the RHI, ECS registry, camera, and the current
    ///   simulation state, plus the @c SelectionManager that tracks the active entity.
    /// - **Scripting** – the hot-reload manager and a non-owning pointer to the
    ///   script manager panel so other panels can trigger reloads.
    /// - **Scene / Render** – scene UBO info and viewport sizing forwarded to the RHI
    ///   each frame.
    /// - **Gizmo** – ImGuizmo operation mode, coordinate space, and snap settings
    ///   shared between @c InspectorPanel (mode buttons) and @c ViewportPanel
    ///   (gizmo rendering).
    /// - **App control** – the @c reloadRequested flag that signals @c Application to
    ///   restart the project.
    /// - **OS drag & drop** – paths dropped onto the GLFW window, consumed by the
    ///   @c ContentBrowser or @c InspectorPanel as appropriate.
    ///
    /// @note Panels must never store raw pointers from this struct beyond a single frame,
    ///       as hot-reload and scene transitions may invalidate them.
    struct EditorContext
    {
        /// @brief Returns the single @c EditorContext instance.
        ///
        /// The instance is constructed on first call and lives until program exit.
        ///
        /// @returns Reference to the singleton @c EditorContext.
        static EditorContext& get()
        {
            static EditorContext instance;
            return instance;
        }

        // ── Core ──────────────────────────────────────────────────────────────

        /// @brief Non-owning pointer to the active Vulkan RHI instance.
        rhi::vulkan::VulkanRHI*  pRHI            = nullptr;

        /// @brief Non-owning pointer to the active ECS entity registry.
        ECS::Registry*           registry        = nullptr;

        /// @brief Current simulation state (STOPPED, PLAYING, or PAUSED).
        SimulationState          simulationState = SimulationState::STOPPED;

        /// @brief Non-owning pointer to the editor camera.
        Camera*                  camera          = nullptr;

        /// @brief Tracks the currently selected entity across all panels.
        UI::SelectionManager     selection;

        // ── Scripting ─────────────────────────────────────────────────────────────

        /// @brief Manages script hot-reload detection and compilation.
        ScriptHotReloadManager m_scriptManager;

        /// @brief Non-owning pointer to the script manager panel for cross-panel reload triggers.
        panel::ScriptManagerPanel* scriptManagerPanel = nullptr;

        // ── Scene / Render ────────────────────────────────────────────────────

        /// @brief Scene-level UBO data (lights, environment) forwarded to the RHI each frame.
        rhi::vulkan::SceneInfos  sceneInfos;

        /// @brief Editor camera movement speed in world units per second.
        float                    camSpeed    = 5.0f;

        /// @brief Current size of the viewport region in pixels (width × height).
        ImVec2                   viewportSize = { 800.f, 600.f };

        // ── Gizmo (shared between InspectorPanel and ViewportPanel) ─────────────

        /// @brief Active ImGuizmo manipulation mode (TRANSLATE, ROTATE, or SCALE).
        ImGuizmo::OPERATION  gizmoOperation      = ImGuizmo::TRANSLATE;

        /// @brief Active ImGuizmo coordinate space (WORLD or LOCAL).
        ImGuizmo::MODE       gizmoMode           = ImGuizmo::WORLD;

        /// @brief When @c true, gizmo manipulations snap to the configured increments.
        bool                 useSnap             = false;

        /// @brief Snap increment for translation on each axis (X, Y, Z) in world units.
        float                snapTranslation[3]  = { 0.5f, 0.5f, 0.5f };

        /// @brief Snap increment for rotation in degrees.
        float                snapRotation        = 15.0f;

        /// @brief Snap increment for uniform scale.
        float                snapScale           = 0.1f;

        // ── App control ───────────────────────────────────────────────────────────

        /// @brief When @c true, @c Application will reload the current project on the next frame.
        bool                 reloadRequested     = false;

        // ── OS drag & drop ────────────────────────────────────────────────────────

        /// @brief Filesystem paths dropped onto the GLFW window by the OS.
        ///
        /// Populated by the GLFW drop callback and consumed (cleared) by whichever
        /// panel handles the paths (typically @c ContentBrowser or @c InspectorPanel).
        std::vector<std::filesystem::path> droppedPaths;
    };

} // namespace gcep::editor
