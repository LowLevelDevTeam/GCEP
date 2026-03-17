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

        // ── Transform — custom : on n'expose pas eulerRadians et rotation séparément
        reg.registerDrawer<ECS::Transform>([](ECS::Transform& t)
        {
            auto drawVec3 = [](Vector3<float>& v, const char* label, ImVec4 color)
            {
                const float totalWidth  = ImGui::GetContentRegionAvail().x;
                const float labelWidth  = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x;
                const float xyzWidth    = ImGui::CalcTextSize("X").x + 2.0f;
                const float fieldWidth  = (totalWidth - labelWidth - xyzWidth * 3.0f - ImGui::GetStyle().ItemSpacing.x * 5.0f) / 3.0f;

                ImGui::TextColored(color, "%s", label);

                const std::string base = std::string("##") + label;

                ImGui::SameLine();
                ImGui::TextColored({ 0.95f, 0.2f, 0.2f, 1.f }, "X");
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::DragFloat((base + "X").c_str(), &v.x, 0.1f);

                ImGui::SameLine();
                ImGui::TextColored({ 0.2f, 0.85f, 0.2f, 1.f }, "Y");
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::DragFloat((base + "Y").c_str(), &v.y, 0.1f);

                ImGui::SameLine();
                ImGui::TextColored({ 0.2f, 0.5f, 0.95f, 1.f }, "Z");
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                ImGui::DragFloat((base + "Z").c_str(), &v.z, 0.1f);
            };

            drawVec3(t.position,     "Position", { 0.9f, 0.3f, 0.3f, 1.f });
            drawVec3(t.eulerRadians, "Rotation", { 0.3f, 0.85f, 0.3f, 1.f });
            drawVec3(t.scale,        "Scale",    { 0.3f, 0.5f, 0.95f, 1.f });
        });

        // ── PhysicsComponent — custom : les enums nécessitent des Combo
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

        reg.registerDrawer<ECS::Tag>();
    }

} // namespace gcep::editor
