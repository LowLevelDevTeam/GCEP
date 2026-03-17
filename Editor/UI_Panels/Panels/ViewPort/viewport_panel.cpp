#include "viewport_panel.hpp"

#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/Prefab/prefab_system.hpp>
#include <PhysicsWrapper/physics_system.hpp>
#include <Scene/header/scene_manager.hpp>
#include <ECS/Components/transform.hpp>
#include <ECS/Components/point_light_component.hpp>
#include <ECS/Components/spot_light_component.hpp>
#include <Maths/quaternion.hpp>
#include <font_awesome.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace gcep::panel
{
    void ViewportPanel::draw()
    {
        handleGizmoInput();

        ImGui::Begin("Viewport", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);

        const ImGuiIO& io = ImGui::GetIO();
        ImGui::PushFont(io.Fonts->Fonts[0]);
        drawToolbar();
        ImGui::PopFont();

        auto& ctx      = editor::EditorContext::get();
        ImVec2 availSize = ImGui::GetContentRegionAvail();

        const bool sizeChanged = std::abs(availSize.x - m_viewportSize.x) > 1.0f ||
                                 std::abs(availSize.y - m_viewportSize.y) > 1.0f;
        bool loading = false;

        if (sizeChanged && availSize.x > 0 && availSize.y > 0)
        {
            m_viewportSize   = availSize;
            ctx.viewportSize = availSize;
            ctx.pRHI->requestOffscreenResize(
                static_cast<uint32_t>(availSize.x),
                static_cast<uint32_t>(availSize.y)
            );
            loading = true;
        }

        if (!loading && m_viewportTexture && *m_viewportTexture != VK_NULL_HANDLE && availSize.x > 0 && availSize.y > 0)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(*m_viewportTexture), availSize);
        }
        else
        {
            ImGui::Text("Viewport loading...");
        }

        // Drop target: accept .gcprefab files dragged from the ContentBrowser
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                std::filesystem::path droppedPath = static_cast<const char*>(payload->Data);
                if (droppedPath.extension() == ".gcprefab")
                {
                    auto& scene = SLS::SceneManager::instance().current();
                    const glm::vec3 spawnPos = ctx.camera->m_position + ctx.camera->m_front * 4.0f;
                    const ECS::EntityID spawnedId = editor::PrefabSystem::instantiate(scene, ctx.pRHI, droppedPath, spawnPos);
                    if (spawnedId != ECS::INVALID_VALUE)
                        ctx.selection.select(spawnedId);
                }
            }
            ImGui::EndDragDropTarget();
        }

        const ImVec2 windowPos  = ImGui::GetWindowPos();
        const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImGuizmo::SetRect(
            windowPos.x + contentMin.x,
            windowPos.y + contentMin.y,
            availSize.x,
            availSize.y
        );

        if (ctx.simulationState != SimulationState::PLAYING)
            drawGizmo();

        ImGui::End();
    }

    void ViewportPanel::drawToolbar()
    {
        auto& ctx     = editor::EditorContext::get();
        auto& physics = PhysicsSystem::getInstance();

        ImGui::BeginMenuBar();

        if (ImGui::Button((std::string(ICON_FA_PLAY) + " Play").c_str()))
        {
            ctx.simulationState = SimulationState::PLAYING;
            ctx.pRHI->setSimulationStarted(true);
            physics.setRegistry(&SLS::SceneManager::instance().current().getRegistry());
            physics.startSimulation();
        }
        ImGui::SameLine();

        if (ImGui::Button((std::string(ICON_FA_PAUSE) + " Pause").c_str()))
        {
            if (ctx.simulationState == SimulationState::PLAYING)
            {
                ctx.simulationState = SimulationState::PAUSED;
                ctx.pRHI->setSimulationStarted(false);
            }
            else if (ctx.simulationState == SimulationState::PAUSED)
            {
                ctx.simulationState = SimulationState::PLAYING;
                ctx.pRHI->setSimulationStarted(true);
            }
        }
        ImGui::SameLine();

        if (ImGui::Button((std::string(ICON_FA_STOP) + " Stop").c_str())
            && ctx.simulationState != SimulationState::STOPPED)
        {
            ctx.simulationState = SimulationState::STOPPED;
            ctx.pRHI->setSimulationStarted(false);
            physics.stopSimulation();
        }
        ImGui::SameLine();

        const char* simLabel = "Stopped";
        if      (ctx.simulationState == SimulationState::PLAYING) simLabel = "Playing";
        else if (ctx.simulationState == SimulationState::PAUSED)  simLabel = "Paused";
        ImGui::Text("Simulation: %s", simLabel);

        // Physics tick while playing
        if (ctx.simulationState == SimulationState::PLAYING)
            physics.update(ImGui::GetIO().DeltaTime);

        ImGui::EndMenuBar();
    }

    void ViewportPanel::handleGizmoInput()
    {
        auto& ctx = editor::EditorContext::get();

        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || ImGuizmo::IsUsing())
            return;
        if (ctx.simulationState == SimulationState::PLAYING)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_W)) ctx.gizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) ctx.gizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) ctx.gizmoOperation = ImGuizmo::SCALE;
    }

    void ViewportPanel::drawGizmo()
    {
        auto& ctx = editor::EditorContext::get();

        if (!ctx.selection.hasSelectedEntity())
            return;

        const ECS::EntityID id = ctx.selection.getSelectedEntityID();

        auto* mesh  = ctx.pRHI->findMesh(id);
        auto* spot  = ctx.pRHI->getLightSystem().findSpotLight(id);
        auto* point = ctx.pRHI->getLightSystem().findPointLight(id);

        if (spot)  { drawGizmoSpotLight(*spot);   return; }
        if (point) { drawGizmoPointLight(*point); return; }
        if (mesh)  { drawGizmoMesh(*mesh);        return; }

        // Generic entity — draw gizmo from ECS Transform directly
        if (ctx.registry->hasComponent<ECS::Transform>(id))
            drawGizmoTransform(id);
    }

    void ViewportPanel::drawGizmoMesh(rhi::vulkan::Mesh& mesh)
    {
        auto& ctx  = editor::EditorContext::get();
        auto& tc   = ctx.registry->getComponent<ECS::Transform>(mesh.id);

        glm::mat4 view       = ctx.camera->getViewMatrix();
        glm::mat4 projection = ctx.camera->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        glm::mat4 modelMatrix = mesh.getTransform();
        float modelData[16];
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);

        float  snapValues[3] = {};
        float* snapPtr       = nullptr;
        if (ctx.useSnap)
        {
            switch (ctx.gizmoOperation)
            {
                case ImGuizmo::TRANSLATE: memcpy(snapValues, ctx.snapTranslation, sizeof(snapValues)); break;
                case ImGuizmo::ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = ctx.snapRotation; break;
                case ImGuizmo::SCALE:     snapValues[0] = snapValues[1] = snapValues[2] = ctx.snapScale;    break;
                default: break;
            }
            snapPtr = snapValues;
        }

        const glm::vec3 oldRotation = { tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z };
        const glm::vec3 oldScale    = { tc.scale.x,        tc.scale.y,        tc.scale.z        };

        float     deltaData[16];
        glm::mat4 identityDelta = glm::mat4(1.0f);
        memcpy(deltaData, glm::value_ptr(identityDelta), sizeof(float) * 16);

        const ImGuizmo::MODE activeMode = (ctx.gizmoOperation == ImGuizmo::SCALE)
            ? ImGuizmo::LOCAL : ctx.gizmoMode;

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            ctx.gizmoOperation, activeMode,
            modelData, deltaData, snapPtr
        );

        if (!ImGuizmo::IsUsing())
            return;

        glm::mat4 newModel, delta;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);
        memcpy(glm::value_ptr(delta),    deltaData, sizeof(float) * 16);

        switch (ctx.gizmoOperation)
        {
            case ImGuizmo::TRANSLATE:
                tc.position = { newModel[3].x, newModel[3].y, newModel[3].z };
                break;

            case ImGuizmo::ROTATE:
            {
                glm::vec3 position, scale, skew;
                glm::vec4 perspective;
                glm::quat rotation;
                glm::decompose(newModel, scale, rotation, position, skew, perspective);
                tc.position     = { position.x, position.y, position.z };
                tc.rotation     = Quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
                tc.rotation.Normalize();
                tc.eulerRadians = {
                    glm::eulerAngles(rotation).x,
                    glm::eulerAngles(rotation).y,
                    glm::eulerAngles(rotation).z
                };
                break;
            }

            case ImGuizmo::SCALE:
            {
                const glm::vec3 deltaScale = {
                    glm::length(glm::vec3(delta[0])),
                    glm::length(glm::vec3(delta[1])),
                    glm::length(glm::vec3(delta[2]))
                };
                const glm::vec3 newScale = oldScale * deltaScale;
                tc.scale        = { newScale.x,    newScale.y,    newScale.z    };
                tc.eulerRadians = { oldRotation.x, oldRotation.y, oldRotation.z };
                break;
            }
            default: break;
        }
    }

    void ViewportPanel::drawGizmoPointLight(rhi::vulkan::PointLight& light)
    {
        auto& ctx = editor::EditorContext::get();
        auto& pos = ctx.registry->getComponent<ECS::PointLightComponent>(light.id).position;

        glm::mat4 view       = ctx.camera->getViewMatrix();
        glm::mat4 projection = ctx.camera->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
        float modelData[16], deltaData[16];
        glm::mat4 identity = glm::mat4(1.0f);
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);
        memcpy(deltaData, glm::value_ptr(identity),    sizeof(float) * 16);

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
            modelData, deltaData, nullptr
        );

        if (!ImGuizmo::IsUsing()) return;

        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);
        pos            = { newModel[3].x, newModel[3].y, newModel[3].z };
        light.position = { newModel[3].x, newModel[3].y, newModel[3].z };
    }

    void ViewportPanel::drawGizmoSpotLight(rhi::vulkan::SpotLight& light)
    {
        auto& ctx    = editor::EditorContext::get();
        auto& oldDir = ctx.registry->getComponent<ECS::SpotLightComponent>(light.id).direction;
        auto& pos    = ctx.registry->getComponent<ECS::SpotLightComponent>(light.id).position;

        glm::mat4 view       = ctx.camera->getViewMatrix();
        glm::mat4 projection = ctx.camera->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        const glm::vec3 dir = glm::normalize(glm::vec3(oldDir.x, oldDir.y, oldDir.z));
        const glm::quat orientQuat = (glm::abs(glm::dot(dir, glm::vec3(0.f, 1.f, 0.f))) < 0.999f)
            ? glm::quatLookAt(dir, glm::vec3(0.f, 1.f, 0.f))
            : glm::quatLookAt(dir, glm::vec3(0.f, 0.f, 1.f));

        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z))
                              * glm::mat4_cast(orientQuat);

        float modelData[16], deltaData[16];
        glm::mat4 identity = glm::mat4(1.0f);
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);
        memcpy(deltaData, glm::value_ptr(identity),    sizeof(float) * 16);

        float  snapValues[3] = {};
        float* snapPtr       = nullptr;
        if (ctx.useSnap)
        {
            switch (ctx.gizmoOperation)
            {
                case ImGuizmo::TRANSLATE: memcpy(snapValues, ctx.snapTranslation, sizeof(snapValues)); break;
                case ImGuizmo::ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = ctx.snapRotation; break;
                default: break;
            }
            snapPtr = snapValues;
        }

        const ImGuizmo::MODE activeMode = (ctx.gizmoOperation == ImGuizmo::SCALE)
            ? ImGuizmo::LOCAL : ctx.gizmoMode;

        const ImGuizmo::OPERATION op = (ctx.gizmoOperation == ImGuizmo::SCALE)
            ? ImGuizmo::TRANSLATE : ctx.gizmoOperation;

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            op, activeMode, modelData, deltaData, snapPtr
        );

        if (!ImGuizmo::IsUsing()) return;

        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);

        switch (ctx.gizmoOperation)
        {
            case ImGuizmo::TRANSLATE:
                pos            = { newModel[3].x, newModel[3].y, newModel[3].z };
                light.position = { newModel[3].x, newModel[3].y, newModel[3].z };
                break;

            case ImGuizmo::ROTATE:
            {
                glm::vec3 position, scale, skew;
                glm::vec4 perspective;
                glm::quat rotation;
                glm::decompose(newModel, scale, rotation, position, skew, perspective);
                pos            = { position.x, position.y, position.z };
                light.position = { position.x, position.y, position.z };
                const glm::vec3 newDir = glm::normalize(rotation * glm::vec3(0.f, 0.f, -1.f));
                oldDir             = { newDir.x, newDir.y, newDir.z };
                light.direction    = { newDir.x, newDir.y, newDir.z };
                break;
            }
            default: break;
        }
    }

    void ViewportPanel::drawGizmoTransform(ECS::EntityID id)
    {
        auto& ctx = editor::EditorContext::get();
        auto& tc  = ctx.registry->getComponent<ECS::Transform>(id);

        glm::mat4 view       = ctx.camera->getViewMatrix();
        glm::mat4 projection = ctx.camera->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        const glm::vec3 pos   = { tc.position.x, tc.position.y, tc.position.z };
        const glm::quat rot   = { tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z };
        const glm::vec3 scale = { tc.scale.x,    tc.scale.y,    tc.scale.z    };

        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), pos)
                              * glm::mat4_cast(rot)
                              * glm::scale(glm::mat4(1.0f), scale);

        float modelData[16], deltaData[16];
        glm::mat4 identity = glm::mat4(1.0f);
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);
        memcpy(deltaData, glm::value_ptr(identity),    sizeof(float) * 16);

        float  snapValues[3] = {};
        float* snapPtr       = nullptr;
        if (ctx.useSnap)
        {
            switch (ctx.gizmoOperation)
            {
                case ImGuizmo::TRANSLATE: memcpy(snapValues, ctx.snapTranslation, sizeof(snapValues)); break;
                case ImGuizmo::ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = ctx.snapRotation; break;
                case ImGuizmo::SCALE:     snapValues[0] = snapValues[1] = snapValues[2] = ctx.snapScale;    break;
                default: break;
            }
            snapPtr = snapValues;
        }

        const ImGuizmo::MODE activeMode = (ctx.gizmoOperation == ImGuizmo::SCALE)
            ? ImGuizmo::LOCAL : ctx.gizmoMode;

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            ctx.gizmoOperation, activeMode,
            modelData, deltaData, snapPtr
        );

        if (!ImGuizmo::IsUsing())
            return;

        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);

        switch (ctx.gizmoOperation)
        {
            case ImGuizmo::TRANSLATE:
                tc.position = { newModel[3].x, newModel[3].y, newModel[3].z };
                break;

            case ImGuizmo::ROTATE:
            {
                glm::vec3 p, s, skew;
                glm::vec4 perspective;
                glm::quat q;
                glm::decompose(newModel, s, q, p, skew, perspective);
                tc.position     = { p.x, p.y, p.z };
                tc.rotation     = Quaternion(q.w, q.x, q.y, q.z);
                tc.rotation.Normalize();
                tc.eulerRadians = { glm::eulerAngles(q).x, glm::eulerAngles(q).y, glm::eulerAngles(q).z };
                break;
            }

            case ImGuizmo::SCALE:
            {
                glm::mat4 delta;
                memcpy(glm::value_ptr(delta), deltaData, sizeof(float) * 16);
                const glm::vec3 deltaScale = {
                    glm::length(glm::vec3(delta[0])),
                    glm::length(glm::vec3(delta[1])),
                    glm::length(glm::vec3(delta[2]))
                };
                tc.scale = { scale.x * deltaScale.x, scale.y * deltaScale.y, scale.z * deltaScale.z };
                break;
            }
            default: break;
        }
    }

} // namespace gcep::panel
