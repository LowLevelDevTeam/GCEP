#pragma once

// Internals
#include <ECS/headers/entity_component.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>

namespace gcep::panel
{
    /// @brief Inline scripting section drawn inside InspectorPanel, below physics.
    /// Does NOT own state — operates on a shared ScriptState reference.
    /// Not an IPanel — call drawEntityScriptSection() directly from InspectorPanel::draw().
    class EntityScriptPanel
    {
    public:
        explicit EntityScriptPanel(ScriptState& state) : m_state(state) {}

        void drawEntityScriptSection();

    private:
        void createAndAttach(ECS::EntityID entityId);
        void attachExisting (ECS::EntityID entityId, const std::string& scriptName);
        void registerEntry  (ECS::EntityID entityId, const std::string& scriptName);
        void destroyInstance(AttachedScript& entry);
        void reloadScriptOnEntity(ECS::EntityID eid, AttachedScript& entry);

        ScriptState& m_state;
    };

} // namespace gcep::panel
