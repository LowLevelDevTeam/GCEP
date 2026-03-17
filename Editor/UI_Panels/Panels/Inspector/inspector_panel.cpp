#include "inspector_panel.hpp"

#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_registry.hpp>
#include <Editor/SceneUtils/scene_util.hpp>
#include <Editor/Prefab/prefab_system.hpp>
#include <Scene/header/scene_manager.hpp>
#include <ECS/Components/hierarchy_component.hpp>
#include <config.hpp>
#include <tinyfiledialogs.h>
#include <font_awesome.hpp>
#include <imgui.h>

#include <Engine/Core/ECS/Components/transform.hpp>

#include <filesystem>
#include <fstream>

namespace gcep::panel
{
    void InspectorPanel::draw()
    {
        auto& ctx = editor::EditorContext::get();

        ImGui::Begin("Entity properties");

        if (!ctx.selection.hasSelectedEntity())
        {
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        const ECS::EntityID id = ctx.selection.getSelectedEntityID();
        auto& reg = editor::InspectorRegistry::get();

        reg.drawComponentAdder(*ctx.registry, id);
        ImGui::SameLine();
        reg.drawComponentRemover(*ctx.registry, id);

        reg.drawComponent<ECS::Transform>(*ctx.registry, id);

        rhi::vulkan::Mesh* mesh = ctx.pRHI->findMesh(id);
        const bool hasLight = ctx.pRHI->getLightSystem().findSpotLight(id) ||
                              ctx.pRHI->getLightSystem().findPointLight(id);
        if (mesh)
        {
            if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                drawRHIExtras(mesh);
        }
        else if (!hasLight)
        {
            if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                drawAttachMesh(id);
        }

        reg.drawAllExcept<ECS::Transform>(*ctx.registry, id);

        ImGui::SeparatorText("Entity actions");
        drawEntityActions(id, mesh);

        if (ctx.simulationState != SimulationState::PLAYING)
        {
            ImGui::SeparatorText("Gizmo controls");
            drawGizmoControls();
        }

        ImGui::End();
    }

    void InspectorPanel::drawGizmoControls()
    {
        auto& ctx = editor::EditorContext::get();

        const char* gizmoOpLabels[]   = { "Translate (W)", "Rotate (E)", "Scale (R)" };
        const char* gizmoModeLabels[] = { "Local", "World" };

        int currentOp = 0;
        switch (ctx.gizmoOperation)
        {
            case ImGuizmo::TRANSLATE: currentOp = 0; break;
            case ImGuizmo::ROTATE:    currentOp = 1; break;
            case ImGuizmo::SCALE:     currentOp = 2; break;
            default:                                 break;
        }

        int currentMode = (ctx.gizmoMode == ImGuizmo::LOCAL) ? 0 : 1;

        if (ImGui::Combo("Operation", &currentOp, gizmoOpLabels, IM_ARRAYSIZE(gizmoOpLabels)))
        {
            switch (currentOp)
            {
                case 0: ctx.gizmoOperation = ImGuizmo::TRANSLATE; break;
                case 1: ctx.gizmoOperation = ImGuizmo::ROTATE;    break;
                case 2: ctx.gizmoOperation = ImGuizmo::SCALE;     break;
                default: break;
            }
        }

        if (ctx.gizmoOperation != ImGuizmo::SCALE)
        {
            if (ImGui::Combo("Mode", &currentMode, gizmoModeLabels, IM_ARRAYSIZE(gizmoModeLabels)))
                ctx.gizmoMode = (currentMode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }

        ImGui::Checkbox("Snap", &ctx.useSnap);
        if (ctx.useSnap)
        {
            switch (ctx.gizmoOperation)
            {
                case ImGuizmo::TRANSLATE: ImGui::DragFloat3("Snapping",       ctx.snapTranslation);                        break;
                case ImGuizmo::ROTATE:    ImGui::DragFloat("Snap Rotation",  &ctx.snapRotation, 1.0f, 0.0f, 360.0f);     break;
                case ImGuizmo::SCALE:     ImGui::DragFloat("Snap Scale",     &ctx.snapScale,    0.01f, 0.0f, 1.0f);      break;
                default: break;
            }
        }
    }

    void InspectorPanel::drawRHIExtras(rhi::vulkan::Mesh* mesh)
    {
        auto& ctx = editor::EditorContext::get();

        ImGui::SeparatorText("Texture");
        if (ImGui::Button("Add texture"))
        {
            const char* filters[] = { "*.png", "*.jpg", "*.jpeg" };
            char* path = tinyfd_openFileDialog("Choose a texture",
                std::filesystem::current_path().string().c_str(), 3, filters, "Image files", 0);
            if (path)
                ctx.pRHI->uploadTexture(ctx.selection.getSelectedEntityID(), path, false);
        }
        if (mesh->hasTexture() && mesh->hasMipmaps())
        {
            static float lodLevel = 0.0f;
            if (ImGui::SliderFloat("LOD Bias", &lodLevel, 0.0f, mesh->texture()->getMipLevels()))
                mesh->texture()->setLodLevel(lodLevel);
        }

        ImGui::SeparatorText("Scripting");
        if (ImGui::Button((std::string(ICON_FA_CODE) + " Add script").c_str()))
        {
            const std::string modelPath  = std::string(PROJECT_ROOT) + "/Engine/Core/Scripting/ScriptModel.cpp";
            const std::string outputPath = std::string(PROJECT_ROOT) + "/Scripts/Script"
                                         + std::to_string(mesh->id) + ".cpp";

            std::filesystem::create_directories(std::string(PROJECT_ROOT) + "/Scripts");

            std::ifstream scriptModel(modelPath, std::ios::binary);
            if (!scriptModel.is_open())
            {
                std::cerr << "Failed to open script model: " << modelPath << std::endl;
                return;
            }

            std::string content((std::istreambuf_iterator<char>(scriptModel)),
                                  std::istreambuf_iterator<char>());

            const std::string scriptName = "Script" + std::to_string(mesh->id);
            auto pos = content.find("SCRIPT_NAME");
            if (pos != std::string::npos)
                content.replace(pos, 11, scriptName);

            std::ofstream out(outputPath, std::ios::binary);
            if (out.is_open())
                out << content;
        }
    }

    void InspectorPanel::drawAttachMesh(ECS::EntityID id)
    {
        auto& ctx = editor::EditorContext::get();

        if (ImGui::BeginMenu("Attach primitive"))
        {
            auto& scene = SLS::SceneManager::instance().current();

            if (ImGui::MenuItem("Cube"))      editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/cube.obj");
            if (ImGui::MenuItem("Sphere"))    editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/sphere.obj");
            if (ImGui::MenuItem("Cylinder"))  editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/cylinder.obj");
            if (ImGui::MenuItem("Cone"))      editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/cone.obj");
            if (ImGui::MenuItem("Icosphere")) editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/icosphere.obj");
            if (ImGui::MenuItem("Torus"))     editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/torus.obj");
            if (ImGui::MenuItem("Suzanne"))   editor::attachMesh(scene, ctx.pRHI, id, "Assets/Models/suzanne.obj");
            ImGui::EndMenu();
        }

        if (ImGui::Button("Attach asset..."))
        {
            const char* filters[] = { "*.obj", "*.gltf" };
            const char* path = tinyfd_openFileDialog("Choose an asset",
                std::filesystem::current_path().string().c_str(), 2, filters, "3D Object files", 0);
            if (path)
            {
                auto& scene = SLS::SceneManager::instance().current();
                editor::attachMesh(scene, ctx.pRHI, id, path);
            }
        }
    }

    void InspectorPanel::drawEntityActions(ECS::EntityID id, rhi::vulkan::Mesh* mesh)
    {
        auto& ctx   = editor::EditorContext::get();
        auto& scene = SLS::SceneManager::instance().current();

        // Save as prefab
        if (ImGui::Button((std::string(ICON_FA_FLOPPY_O) + " Save as Prefab...").c_str()))
        {
            const char* filters[] = { "*.gcprefab" };
            const char* outPath = tinyfd_saveFileDialog("Save Prefab",
                std::filesystem::current_path().string().c_str(), 1, filters, "GCEP Prefab");
            if (outPath)
            {
                std::string meshPath;
                if (mesh) meshPath = mesh->objPath().string();
                editor::PrefabSystem::save(*ctx.registry, id, meshPath, outPath);
            }
        }

        // Add child
        ImGui::SameLine();
        if (ImGui::Button((std::string(ICON_FA_PLUS) + " Add child").c_str()))
        {
            const ECS::EntityID child = scene.createEntity("Entity");
            scene.setParent(child, id);
            ctx.selection.select(child);
        }

        // Detach from parent — only shown when entity has a parent
        if (ctx.registry->hasComponent<ECS::HierarchyComponent>(id))
        {
            const auto& hier = ctx.registry->getComponent<ECS::HierarchyComponent>(id);
            if (hier.parent != ECS::INVALID_VALUE)
            {
                ImGui::SameLine();
                if (ImGui::Button((std::string(ICON_FA_CHAIN_BROKEN) + " Detach from parent").c_str()))
                    scene.removeParent(id);
            }
        }
    }

} // namespace gcep::panel
