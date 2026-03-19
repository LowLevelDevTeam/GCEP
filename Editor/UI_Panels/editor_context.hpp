#pragma once

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
    struct EditorContext
    {
        static EditorContext& get()
        {
            static EditorContext instance;
            return instance;
        }

        // ── Core ──────────────────────────────────────────────────────────────
        rhi::vulkan::VulkanRHI*  pRHI            = nullptr;
        ECS::Registry*           registry        = nullptr;
        SimulationState          simulationState = SimulationState::STOPPED;
        Camera*                  camera          = nullptr;
        UI::SelectionManager     selection;

        // ── Scripting ─────────────────────────────────────────────────────────────
        ScriptHotReloadManager m_scriptManager;
        panel::ScriptManagerPanel* scriptManagerPanel = nullptr;

        // ── Scene / Render ────────────────────────────────────────────────────
        rhi::vulkan::SceneInfos  sceneInfos;
        float                    camSpeed    = 5.0f;
        ImVec2                   viewportSize = { 800.f, 600.f };

        // ── Gizmo (partagé entre InspectorPanel et ViewportPanel) ─────────────
        ImGuizmo::OPERATION  gizmoOperation      = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE       gizmoMode           = ImGuizmo::WORLD;
        bool                 useSnap             = false;
        float                snapTranslation[3]  = { 0.5f, 0.5f, 0.5f };
        float                snapRotation        = 15.0f;
        float                snapScale           = 0.1f;

        // ── App control ───────────────────────────────────────────────────────────
        bool                 reloadRequested     = false;

        // ── OS drag & drop ────────────────────────────────────────────────────────
        std::vector<std::filesystem::path> droppedPaths;
    };

} // namespace gcep::editor
