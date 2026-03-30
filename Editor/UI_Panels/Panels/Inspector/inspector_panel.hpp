#pragma once

/// @file inspector_panel.hpp
/// @brief Dockable component property editor for the currently selected entity.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.0
/// @date 2026-02-17

// Internals
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_registry.hpp>
#include <Editor/UI_Panels/Panels/Scripting/entity_script_panel.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>

namespace gcep::panel
{
    /// @class InspectorPanel
    /// @brief Dear ImGui panel that displays and edits the components of the selected entity.
    ///
    /// @c InspectorPanel reads the active selection from @c EditorContext::selection and
    /// uses @c InspectorRegistry to enumerate and render all ECS components attached to
    /// that entity. It also provides:
    /// - Gizmo mode / space controls (translate, rotate, scale; world vs. local).
    /// - RHI-level extras for entities that own an @c rhi::vulkan::Mesh (material slot,
    ///   mesh-swap button).
    /// - Quick actions for the selected entity (duplicate, destroy).
    /// - A dedicated @c EntityScriptPanel section for attaching and editing Lua scripts.
    ///
    /// @par Ownership
    /// @c InspectorPanel owns the @c EntityScriptPanel member by value and forwards the
    /// caller-supplied @c ScriptState reference to it at construction.
    ///
    /// @note Must be called from the main thread within an active Dear ImGui frame.
    class InspectorPanel : public IPanel
    {
    public:
        /// @brief Constructs the inspector and wires @p scriptState into the script sub-panel.
        ///
        /// @param scriptState  Reference to the shared hot-reload script state.
        ///                     Must outlive this panel instance.
        explicit InspectorPanel(ScriptState& scriptState)
        : m_scriptPanel(scriptState) {}

        /// @brief Renders the inspector panel for the current frame.
        ///
        /// If no entity is selected (@c EditorContext::selection has no valid ID), the
        /// panel displays a placeholder message. Otherwise it renders, in order:
        /// @c drawGizmoControls(), the @c InspectorRegistry component list,
        /// @c drawRHIExtras(), @c drawAttachMesh(), @c drawEntityActions(), and the
        /// @c EntityScriptPanel section.
        void draw() override;

    private:
        /// @brief Renders the gizmo operation and coordinate-space toggle buttons.
        ///
        /// Writes the user's choice directly into @c EditorContext::gizmoOperation and
        /// @c EditorContext::gizmoMode so @c ViewportPanel picks them up the same frame.
        void drawGizmoControls();

        /// @brief Renders RHI-specific properties for an entity that owns a mesh.
        ///
        /// Shows the assigned material path, a drag-and-drop slot to reassign it, and
        /// any mesh-level render flags (cast shadows, etc.).
        ///
        /// @param mesh  Pointer to the RHI mesh component of the selected entity.
        ///              May be @c nullptr if the entity has no mesh; in that case the
        ///              function returns immediately.
        void drawRHIExtras(rhi::vulkan::Mesh* mesh);

        /// @brief Renders a drag-and-drop slot for attaching a mesh asset to the entity.
        ///
        /// Accepts a @c "CONTENT_BROWSER_ITEM" payload whose path points to a supported
        /// mesh format. On successful drop, loads and binds the mesh via the RHI.
        ///
        /// @param id  ECS entity identifier of the currently selected entity.
        static void drawAttachMesh(ECS::EntityID id);

        /// @brief Renders common entity action buttons (Duplicate, Destroy).
        ///
        /// Destroy queues entity removal via the ECS registry; if the entity also owns
        /// @p mesh, the corresponding RHI mesh is released first.
        ///
        /// @param id    ECS entity identifier of the currently selected entity.
        /// @param mesh  Pointer to the RHI mesh owned by the entity, or @c nullptr.
        static void drawEntityActions(ECS::EntityID id, rhi::vulkan::Mesh* mesh);

        /// @brief Embedded sub-panel for attaching, detaching, and editing entity scripts.
        EntityScriptPanel m_scriptPanel;
    };

} // namespace gcep::panel
