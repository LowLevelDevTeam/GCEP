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
                const float btnH = ImGui::GetFrameHeight();
                ImGui::Button(text, ImVec2(0.f, btnH));
                ImGui::PopStyleColor(4);
            };

            const float columnX = std::max({ ImGui::CalcTextSize("Position").x,
                                             ImGui::CalcTextSize("Rotation").x,
                                             ImGui::CalcTextSize("Scale").x })
                                  + ImGui::GetStyle().FramePadding.x * 2.0f
                                  + ImGui::GetStyle().ItemSpacing.x;

            constexpr ImVec4 grey = { 0.22f, 0.22f, 0.22f, 1.f };

            // Draws a Vec3 row and returns true if any component was changed.
            auto drawVec3 = [&colorLabel, &white, &grey, columnX](Vector3<float>& v, const char* label) -> bool
            {
                const float spacing    = ImGui::GetStyle().ItemSpacing.x;
                const float btnW       = ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const float totalWidth = ImGui::GetContentRegionAvail().x;
                const float fieldWidth = (totalWidth - columnX - (btnW + 2.0f) * 3.0f - spacing * 2.0f) / 3.0f;

                colorLabel(label, grey, white);

                const std::string base = std::string("##") + label;
                bool changed = false;

                ImGui::SameLine(columnX);
                colorLabel(("X##bx_" + base).c_str(), { 0.85f, 0.15f, 0.15f, 1.f }, white);
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                changed |= ImGui::DragFloat((base + "X").c_str(), &v.x, 0.1f);

                ImGui::SameLine();
                colorLabel(("Y##by_" + base).c_str(), { 0.15f, 0.75f, 0.15f, 1.f }, white);
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                changed |= ImGui::DragFloat((base + "Y").c_str(), &v.y, 0.1f);

                ImGui::SameLine();
                colorLabel(("Z##bz_" + base).c_str(), { 0.15f, 0.35f, 0.85f, 1.f }, white);
                ImGui::SameLine(0, 2);
                ImGui::SetNextItemWidth(fieldWidth);
                changed |= ImGui::DragFloat((base + "Z").c_str(), &v.z, 0.1f);

                return changed;
            };

            // Sync display cache from the authoritative quaternion every frame
            // so the UI always reflects the true rotation (e.g. after gizmo edits).
            {
                const glm::quat q(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);
                const glm::vec3 e = glm::eulerAngles(q);
                t.eulerRadians = { e.x, e.y, e.z };
            }

            drawVec3(t.position, "Position");
            drawVec3(t.scale,    "Scale");

            // Rotation: edit eulerRadians in the UI, but always write back through
            // the quaternion so t.rotation stays the single source of truth.
            if (drawVec3(t.eulerRadians, "Rotation"))
            {
                const glm::quat q = glm::quat(glm::vec3(t.eulerRadians.x, t.eulerRadians.y, t.eulerRadians.z));
                t.rotation = Quaternion(q.w, q.x, q.y, q.z);
                t.rotation.Normalize();
                const glm::vec3 e = glm::eulerAngles(q);
                t.eulerRadians = { e.x, e.y, e.z };
            }
        }, /*removable=*/false);

        reg.registerDrawer<ECS::PhysicsComponent>([](ECS::PhysicsComponent& p)
        {
            const char* motionLabels[] = { "Static", "Dynamic", "Kinematic" };
            const char* shapeLabels[]  = { "Cube", "Sphere", "Cylinder", "Capsule" };

            int motion = static_cast<int>(p.motionType);
            int shape  = static_cast<int>(p.shapeType);

            if (ImGui::Combo("Motion Type", &motion, motionLabels, IM_ARRAYSIZE(motionLabels)))
                p.motionType = static_cast<ECS::EMotionType>(motion);
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
