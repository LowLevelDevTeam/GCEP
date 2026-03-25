#pragma once

/// @file scene_hierarchy_panel.hpp
/// @brief Dockable entity tree view for the currently loaded scene.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.0
/// @date 2026-02-17

// Internals
#include <ECS/headers/entity_component.hpp>
#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/editor_context.hpp>

// STL
#include <string>

namespace gcep::panel
{
    /// @class SceneHierarchyPanel
    /// @brief Dear ImGui panel that displays the full entity tree of the active scene.
    ///
    /// @c SceneHierarchyPanel iterates the @c ECS::Registry stored in @c EditorContext
    /// and renders each entity as a selectable tree node. Selecting a node writes its
    /// ID into @c EditorContext::selection so the @c InspectorPanel and @c ViewportPanel
    /// can react immediately.
    ///
    /// @par Per-entity display
    /// Each node is labelled by @c getEntityLabel(), which probes the RHI mesh and
    /// light lists for a human-readable name, falling back to the raw entity ID.
    /// A right-click context menu (rendered by @c drawEntityContextMenu()) offers
    /// actions such as Duplicate and Destroy.
    ///
    /// @par Spawn menu
    /// The toolbar "+" button opens @c drawSpawnMenu(), which lists primitive types
    /// (Empty, Mesh, Point Light, Spot Light) that can be spawned into the scene.
    ///
    /// @note Must be called from the main thread within an active Dear ImGui frame.
    class SceneHierarchyPanel : public IPanel
    {
    public:
        /// @brief Renders the scene hierarchy panel for the current frame.
        ///
        /// Opens an ImGui window, then renders @c drawSpawnMenu() followed by a
        /// tree of @c drawEntityNode() calls — one per entity in the registry.
        void draw() override;

    private:
        /// @brief Renders the "+" spawn toolbar button and its entity-type sub-menu.
        ///
        /// Each menu item creates a new entity and the corresponding ECS / RHI
        /// components via @c EditorContext::registry and @c EditorContext::pRHI.
        void drawSpawnMenu();

        /// @brief Renders a single entity as a selectable tree node.
        ///
        /// Shows the label returned by @c getEntityLabel(). Clicking selects the
        /// entity through @c EditorContext::selection. Right-clicking opens the
        /// context menu via @c drawEntityContextMenu().
        ///
        /// @param id  ECS entity identifier of the entity to render.
        void drawEntityNode(ECS::EntityID id);

        /// @brief Renders the right-click context menu for a specific entity node.
        ///
        /// Provides actions: Rename (in-place), Duplicate, and Destroy.
        /// Destroy calls @c removeSelected() after clearing the selection.
        ///
        /// @param id  ECS entity identifier of the entity that was right-clicked.
        void drawEntityContextMenu(ECS::EntityID id);

        /// @brief Destroys the currently selected entity and clears the selection.
        ///
        /// Removes any associated RHI mesh or light entry before calling
        /// @c ECS::Registry::destroyEntity() to avoid dangling GPU resources.
        void removeSelected();

        /// @brief Builds a human-readable label for @p id.
        ///
        /// Searches the RHI mesh list and light lists for a name associated with
        /// the entity. Falls back to @c "Entity <id>" if no name is found.
        ///
        /// @param id  ECS entity identifier to label.
        /// @returns   A display string in the form "icon  name" (e.g. "⬡  Cube").
        [[nodiscard]] std::string getEntityLabel(ECS::EntityID id) const;
    };

} // namespace gcep::panel
