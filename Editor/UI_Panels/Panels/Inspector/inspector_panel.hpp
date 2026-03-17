#pragma once

#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_registry.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>

namespace gcep::panel
{
    class InspectorPanel : public IPanel
    {
    public:
        void draw() override;

    private:
        void drawGizmoControls();
        void drawRHIExtras(rhi::vulkan::Mesh* mesh);
        static void drawAttachMesh(ECS::EntityID id);
    };

} // namespace gcep::panel
