#pragma once

/// @file selection_manager.hpp
/// @brief Lightweight single-entity selection tracker for the editor.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.0
/// @date 2026-02-17

// Internals
#include <Engine/Core/ECS/Components/transform.hpp>

namespace gcep::UI
{
    /// @class SelectionManager
    /// @brief Tracks the single entity currently selected in the editor.
    ///
    /// @c SelectionManager holds at most one selected @c ECS::EntityID at a time.
    /// It is stored inside @c EditorContext and accessed by any panel that needs to
    /// read or change the active selection (e.g. @c SceneHierarchyPanel to set it,
    /// @c InspectorPanel to read it, @c ViewportPanel to render the gizmo on it).
    ///
    /// @par Invariant
    /// When no entity is selected, @c m_selectedEntityID equals @c ECS::INVALID_VALUE.
    /// @c hasSelectedEntity() is the preferred way to test for a valid selection before
    /// calling @c getSelectedEntityID().
    ///
    /// @note This class is intentionally trivial (no virtual methods, no heap
    ///       allocation) so it can be embedded by value in @c EditorContext.
    class SelectionManager
    {
    public:
        /// @brief Sets @p id as the currently selected entity, replacing any prior selection.
        ///
        /// @param id  ECS entity identifier to select. Must be a valid live entity.
        void select(ECS::EntityID id)
        {
            m_selectedEntityID = id;
        }

        /// @brief Clears the current selection.
        ///
        /// Sets the stored ID to @c ECS::INVALID_VALUE. Subsequent calls to
        /// @c hasSelectedEntity() will return @c false.
        void unselect()
        {
            m_selectedEntityID = ECS::INVALID_VALUE;
        }

        /// @brief Returns the currently selected entity identifier.
        ///
        /// @returns The selected @c ECS::EntityID, or @c ECS::INVALID_VALUE if nothing
        ///          is selected. Callers should check @c hasSelectedEntity() first.
        [[nodiscard]] ECS::EntityID getSelectedEntityID() const { return m_selectedEntityID; }

        /// @brief Returns @c true when a valid entity is selected.
        ///
        /// @returns @c true if the stored ID is not @c ECS::INVALID_VALUE.
        [[nodiscard]] bool hasSelectedEntity() const { return m_selectedEntityID != ECS::INVALID_VALUE; }

    private:
        /// @brief Identifier of the currently selected entity.
        ///
        /// Initialised to @c ECS::INVALID_VALUE (no selection).
        ECS::EntityID m_selectedEntityID = ECS::INVALID_VALUE;
    };

} // namespace gcep::UI
