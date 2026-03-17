#include <Editor/UI_Panels/Panels/Inspector/inspector_registry.hpp>
#include <Editor/UI_Panels/component_drawers.hpp>

#include <ECS/Components/components.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <Maths/quaternion.hpp>
#include <imgui.h>

namespace gcep::editor
{
    void registerEngineDrawers()
    {
        auto& reg = InspectorRegistry::get();

        reg.registerDrawer<ECS::Transform>([](ECS::Transform& t)
        {
            constexpr ImVec4 white = { 1.f, 1.f, 1.f, 1.f };

            auto colorLabel = [&white](const char* text, ImVec4 bg, ImVec4 fg)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  bg);
                ImGui::PushStyleColor(ImGuiCol_Text,          fg);
                ImGui::SmallButton(text);
                ImGui::PopStyleColor(4);
            };

            const float columnX = std::max({ ImGui::CalcTextSize("Position").x,
                                             ImGui::CalcTextSize("Rotation").x,
                                             ImGui::CalcTextSize("Scale").x })
                                  + ImGui::GetStyle().FramePadding.x * 2.0f
                                  + ImGui::GetStyle().ItemSpacing.x;

            auto drawVec3 = [&colorLabel, &white, columnX](Vector3<float>& v, const char* label, ImVec4 color)
            {
                const float spacing    = ImGui::GetStyle().ItemSpacing.x;
                const float btnW       = ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const float totalWidth = ImGui::GetContentRegionAvail().x;
                const float fieldWidth = (totalWidth - columnX - (btnW + 2.0f) * 3.0f - spacing * 2.0f) / 3.0f;

                colorLabel(label, white, color);

                const std::string base = std::string("##") + label;

                ImGui::SameLine(columnX);
                colorLabel("X", { 0.85f, 0.15f, 0.15f, 1.f }, white);
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::DragFloat((base + "X").c_str(), &v.x, 0.1f);

                ImGui::SameLine();
                colorLabel("Y", { 0.15f, 0.75f, 0.15f, 1.f }, white);
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::DragFloat((base + "Y").c_str(), &v.y, 0.1f);

                ImGui::SameLine();
                colorLabel("Z", { 0.15f, 0.35f, 0.85f, 1.f }, white);
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::DragFloat((base + "Z").c_str(), &v.z, 0.1f);
            };

            drawVec3(t.position,     "Position", { 0.9f, 0.3f, 0.3f, 1.f });
            drawVec3(t.eulerRadians, "Rotation", { 0.3f, 0.85f, 0.3f, 1.f });
            drawVec3(t.scale,        "Scale",    { 0.3f, 0.5f, 0.95f, 1.f });
        }, /*removable=*/false);

        reg.registerDrawer<ECS::PhysicsComponent>([](ECS::PhysicsComponent& p)
        {
            const char* motionLabels[] = { "Static", "Dynamic", "Kinematic" };
            const char* layerLabels[]  = { "Non moving", "Moving" };
            const char* shapeLabels[]  = { "Cube", "Sphere", "Cylinder", "Capsule" };

            int motion = static_cast<int>(p.motionType);
            int layer  = static_cast<int>(p.layers);
            int shape  = static_cast<int>(p.shapeType);

            if (ImGui::Combo("Motion Type", &motion, motionLabels, IM_ARRAYSIZE(motionLabels)))
                p.motionType = static_cast<ECS::EMotionType>(motion);
            if (ImGui::Combo("Layer",       &layer,  layerLabels,  IM_ARRAYSIZE(layerLabels)))
                p.layers = static_cast<ECS::ELayers>(layer);
            if (ImGui::Combo("Shape",       &shape,  shapeLabels,  IM_ARRAYSIZE(shapeLabels)))
                p.shapeType = static_cast<ECS::EShapeType>(shape);

            ImGui::Checkbox("Is Trigger", &p.isTrigger);
        });

        reg.registerDrawer<ECS::PointLightComponent>([](ECS::PointLightComponent& p)
        {
            drawField(p.position,  "Position");
            drawField(p.color,     "Color");
            drawField(p.intensity, "Intensity");
            drawField(p.radius,    "Radius");
        });

        reg.registerDrawer<ECS::SpotLightComponent>([](ECS::SpotLightComponent& s)
        {
            drawField(s.position,       "Position");
            drawField(s.direction,      "Direction");
            drawField(s.color,          "Color");
            drawField(s.intensity,      "Intensity");
            drawField(s.radius,         "Radius");
            drawField(s.innerCutoffDeg, "Inner Cutoff");
            drawField(s.outerCutoffDeg, "Outer Cutoff");
            drawField(s.showGizmo,      "Show Gizmo");
        });

        reg.registerDrawer<ECS::Tag>([](ECS::Tag& t)
        {
            drawField(t.name, "Name");
        }, /*removable=*/false);

        reg.registerDrawer<ECS::CameraComponent>([](ECS::CameraComponent& c)
        {
            //drawField(c.fovYDeg,       "FOV Y");
            //drawField(c.nearZ,         "Near Z");
            //drawField(c.farZ,          "Far Z");
            ImGui::Checkbox("Is Main Camera", &c.isMainCamera);
        });
    }

} // namespace gcep::editor
