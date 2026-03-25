#pragma once

// Internals
#include <ECS/headers/entity_component.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>

namespace gcep::panel
{
    /// @brief Inline scripting section drawn inside @c InspectorPanel, below the physics block.
    ///
    /// Renders the "attach script", "reload", and "detach" controls for the entity
    /// that is currently selected in the editor. The panel does not own any state;
    /// it operates entirely on the shared @c ScriptState reference supplied at
    /// construction time.
    ///
    /// @par Integration
    /// This is **not** an @c IPanel — it has no independent docked window. Call
    /// @c drawEntityScriptSection() directly from @c InspectorPanel::draw() at the
    /// point where the scripting block should appear.
    class EntityScriptPanel
    {
    public:
        /// @brief Constructs the panel with a reference to the shared scripting state.
        ///
        /// @param state  Shared @c ScriptState owned by @c UiManager. Must outlive this panel.
        explicit EntityScriptPanel(ScriptState& state) : m_state(state) {}

        /// @brief Draws the scripting UI block for the currently selected entity.
        ///
        /// Renders the combo-box for selecting and attaching a script, an inline
        /// "Reload" button, status feedback, and a "Detach" button if a script is
        /// already attached. Must be called every ImGui frame from within a valid
        /// ImGui window (e.g. @c InspectorPanel::draw()).
        void drawEntityScriptSection();

    private:
        /// @brief Creates a new script source file on disk and attaches it to @p entityId.
        ///
        /// Writes a boilerplate @c .cpp file to @c m_state.scriptsDir, triggers a
        /// compile via the hot-reload manager, then calls @c registerEntry().
        ///
        /// @param entityId  Target entity that will own the new script.
        void createAndAttach(ECS::EntityID entityId);

        /// @brief Attaches an already-compiled script to @p entityId.
        ///
        /// Looks up @p scriptName in the hot-reload manager and, if found and valid,
        /// calls @c registerEntry() to bind it to the entity.
        ///
        /// @param entityId   Target entity.
        /// @param scriptName Name key of the script in @c ScriptHotReloadManager.
        void attachExisting(ECS::EntityID entityId, const std::string& scriptName);

        /// @brief Inserts an @c AttachedScript entry into @c m_state.entityScripts
        ///        and calls @c createInstance() on the hot-reload manager.
        ///
        /// Should only be called once a valid @c LoadedScript exists for @p scriptName.
        ///
        /// @param entityId   Entity to register.
        /// @param scriptName Script to associate with the entity.
        void registerEntry(ECS::EntityID entityId, const std::string& scriptName);

        /// @brief Calls @c onEnd() and destroys the script instance held by @p entry.
        ///
        /// After this call @p entry.runtime is reset and the entity has no live
        /// script instance. The @c AttachedScript itself remains in @c entityScripts
        /// until the caller removes it.
        ///
        /// @param entry  The attached-script record whose instance should be destroyed.
        void destroyInstance(AttachedScript& entry);

        /// @brief Hot-reloads the script attached to @p eid in-place.
        ///
        /// Destroys the existing instance, triggers a library reload through
        /// @c ScriptHotReloadManager::hotReload(), and recreates the instance so
        /// the entity continues running with the updated code.
        ///
        /// @param eid    Entity whose script should be reloaded.
        /// @param entry  The corresponding @c AttachedScript record in @c m_state.entityScripts.
        void reloadScriptOnEntity(ECS::EntityID eid, AttachedScript& entry);

        ScriptState& m_state; ///< Non-owning reference to the shared scripting state.
    };

} // namespace gcep::panel
