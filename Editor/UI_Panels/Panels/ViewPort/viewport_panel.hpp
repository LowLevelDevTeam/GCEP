#pragma once

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
    class ViewportPanel : public IPanel
    {
    public:
        /// @brief Binds the viewport texture descriptor produced by VulkanRHI.
        void setViewportTexture(VkDescriptorSet* texture) { m_viewportTexture = texture; }

        void draw() override;

    private:
        void drawToolbar();
        void drawGizmo();
        void drawGizmoMesh(rhi::vulkan::Mesh& mesh);
        void drawGizmoPointLight(rhi::vulkan::PointLight& light);
        void drawGizmoSpotLight(rhi::vulkan::SpotLight& light);
        void drawGizmoTransform(ECS::EntityID id);
        void handleGizmoInput();
        void startSimulation(editor::EditorContext& ctx, PhysicsSystem& physics);
        void pauseSimulation(editor::EditorContext& ctx);
        void stopSimulation(editor::EditorContext& ctx, PhysicsSystem& physics);

        ImVec2          m_viewportSize    = { 800.f, 600.f };
        VkDescriptorSet* m_viewportTexture = nullptr;
    };

} // namespace gcep::panel
