#pragma once

/// @file viewport_panel.hpp
/// @brief Dockable 3D viewport panel with toolbar, gizmo, and simulation controls.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.0
/// @date 2026-02-17

// Internals
#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>
#include <PhysicsWrapper/physics_system.hpp>

// Externals
#include <imgui.h>
#include <ImGuizmo.h>
#include <vulkan/vulkan.h>

namespace gcep::panel
{
    /// @class ViewportPanel
    /// @brief Dear ImGui panel that displays the live 3D scene and hosts editor tools.
    ///
    /// @c ViewportPanel renders the scene colour buffer produced by @c VulkanRHI as a
    /// full-panel image, then overlays an ImGuizmo transform gizmo and a toolbar
    /// for controlling the in-editor simulation.
    ///
    /// @par Responsibilities
    /// - Displaying the RHI-rendered scene texture via an ImGui image.
    /// - Drawing the play / pause / stop toolbar and forwarding state changes to
    ///   @c EditorContext.
    /// - Rendering the ImGuizmo gizmo for the currently selected entity and
    ///   dispatching per-type helpers (@c drawGizmoMesh, @c drawGizmoPointLight, …).
    /// - Translating raw ImGuizmo deltas into ECS transform / physics updates through
    ///   @c handleGizmoInput().
    ///
    /// @par Simulation lifecycle
    /// The three private helpers @c startSimulation(), @c pauseSimulation(), and
    /// @c stopSimulation() are called from @c drawToolbar() and are responsible for
    /// transitioning @c EditorContext::simulationState accordingly.
    ///
    /// @note The viewport texture must be bound via @c setViewportTexture() before
    ///       the first call to @c draw(); the panel will render a blank image otherwise.
    class ViewportPanel : public IPanel
    {
    public:
        /// @brief Binds the Vulkan descriptor set that holds the rendered scene texture.
        ///
        /// Must be called once after the RHI has produced its first swapchain image.
        /// The pointer is stored as-is; ownership remains with @c VulkanRHI.
        ///
        /// @param texture  Non-owning pointer to the descriptor set wrapping the
        ///                 scene colour attachment.
        void setViewportTexture(VkDescriptorSet* texture) { m_viewportTexture = texture; }

        /// @brief Renders the viewport panel for the current frame.
        ///
        /// Sequence: opens an ImGui window sized to the dockspace slot, draws the
        /// scene image, then overlays @c drawToolbar() and @c drawGizmo().
        void draw() override;

    private:
        /// @brief Renders the play / pause / stop simulation control bar.
        ///
        /// Reads the current @c EditorContext::simulationState and draws the
        /// appropriate enabled / disabled buttons. Calls @c startSimulation(),
        /// @c pauseSimulation(), or @c stopSimulation() on user interaction.
        void drawToolbar();

        /// @brief Dispatches ImGuizmo rendering for the currently selected entity.
        ///
        /// Queries @c EditorContext::selection for the active entity, then delegates
        /// to the appropriate typed helper (@c drawGizmoMesh, @c drawGizmoPointLight,
        /// @c drawGizmoSpotLight, or @c drawGizmoTransform). Calls
        /// @c handleGizmoInput() after the gizmo is drawn.
        void drawGizmo();

        /// @brief Renders the transform gizmo for a mesh entity.
        ///
        /// Reads the mesh world matrix from @p mesh, runs ImGuizmo::Manipulate, then
        /// writes the decomposed translation / rotation / scale back if the gizmo
        /// was manipulated.
        ///
        /// @param mesh  Reference to the RHI mesh whose transform is being edited.
        void drawGizmoMesh(rhi::vulkan::Mesh& mesh);

        /// @brief Renders the transform gizmo for a point light entity.
        ///
        /// @param light  Reference to the RHI point light whose position is being edited.
        void drawGizmoPointLight(rhi::vulkan::PointLight& light);

        /// @brief Renders the transform gizmo for a spot light entity.
        ///
        /// @param light  Reference to the RHI spot light whose transform is being edited.
        void drawGizmoSpotLight(rhi::vulkan::SpotLight& light);

        /// @brief Renders the transform gizmo for an entity that has only an ECS transform.
        ///
        /// Used as a fallback for entities that carry no RHI mesh or light component.
        ///
        /// @param id  ECS entity identifier whose @c Transform component is being edited.
        void drawGizmoTransform(ECS::EntityID id);

        /// @brief Applies the gizmo delta to the ECS and physics state after manipulation.
        ///
        /// Called once per frame after ImGuizmo::Manipulate. Decomposes the updated
        /// matrix and writes the resulting translation / rotation / scale into the
        /// entity's @c Transform component. Also notifies the physics body if a
        /// @c RigidBody component is present.
        void handleGizmoInput();

        /// @brief Transitions the simulation to the PLAYING state.
        ///
        /// Saves the current scene state for later restoration, initialises physics
        /// actors for all entities with @c RigidBody components, and updates
        /// @c EditorContext::simulationState to @c SimulationState::PLAYING.
        ///
        /// @param ctx      Shared editor context whose simulation state is updated.
        /// @param physics  Physics subsystem used to spawn rigid-body actors.
        void startSimulation(editor::EditorContext& ctx, PhysicsSystem& physics);

        /// @brief Transitions the simulation from PLAYING to PAUSED.
        ///
        /// Freezes physics integration without destroying actors or scene state.
        /// Updates @c EditorContext::simulationState to @c SimulationState::PAUSED.
        ///
        /// @param ctx  Shared editor context whose simulation state is updated.
        void pauseSimulation(editor::EditorContext& ctx);

        /// @brief Transitions the simulation back to the STOPPED state.
        ///
        /// Destroys all runtime physics actors, restores the scene to the snapshot
        /// taken at @c startSimulation(), and updates @c EditorContext::simulationState
        /// to @c SimulationState::STOPPED.
        ///
        /// @param ctx      Shared editor context whose simulation state is updated.
        /// @param physics  Physics subsystem used to remove runtime actors.
        void stopSimulation(editor::EditorContext& ctx, PhysicsSystem& physics);

        /// @brief Current size of the viewport region in pixels (width × height).
        ImVec2           m_viewportSize    = { 800.f, 600.f };

        /// @brief Non-owning pointer to the Vulkan descriptor set wrapping the scene texture.
        VkDescriptorSet* m_viewportTexture = nullptr;
    };

} // namespace gcep::panel
