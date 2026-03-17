#pragma once

#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_registry.hpp>
#include <Editor/UI_Panels/Panels/Scripting/entity_script_panel.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>

namespace gcep::panel
{
    class InspectorPanel : public IPanel
    {
    public:
        explicit InspectorPanel(ScriptState& scriptState)
        : m_scriptPanel(scriptState) {}
        void draw() override;

    private:
        void drawGizmoControls();
        void drawRHIExtras(rhi::vulkan::Mesh* mesh);
        static void drawAttachMesh(ECS::EntityID id);
        static void drawEntityActions(ECS::EntityID id, rhi::vulkan::Mesh* mesh);

        EntityScriptPanel m_scriptPanel;
    };

} // namespace gcep::panel
