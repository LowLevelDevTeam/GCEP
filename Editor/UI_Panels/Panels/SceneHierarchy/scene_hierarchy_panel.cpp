#include "scene_hierarchy_panel.hpp"

#include <Editor/UI_Panels/editor_context.hpp>
#include <Editor/SceneUtils/scene_util.hpp>
#include <Editor/Prefab/prefab_system.hpp>
#include <Scene/header/scene_manager.hpp>
#include <ECS/Components/hierarchy_component.hpp>
#include <ECS/Components/tag.hpp>
#include <font_awesome.hpp>
#include <tinyfiledialogs.h>
#include <imgui.h>

#include <filesystem>
#include <ranges>
#include <string>

namespace gcep::panel
{
    std::string SceneHierarchyPanel::getEntityLabel(ECS::EntityID id) const
    {
        auto& ctx = editor::EditorContext::get();

        // Name comes from Tag
        std::string name = std::to_string(id);
        if (ctx.registry->hasComponent<ECS::Tag>(id))
            name = ctx.registry->getComponent<ECS::Tag>(id).name;

        // Icon based on RHI presence
        if (ctx.pRHI->findMesh(id))
            return std::string(ICON_FA_CUBE) + " " + name;

        if (ctx.pRHI->getLightSystem().findPointLight(id) ||
            ctx.pRHI->getLightSystem().findSpotLight(id))
            return std::string(ICON_FA_LIGHTBULB_O) + " " + name;

        return std::string(ICON_FA_CIRCLE_O) + " " + name;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // drawEntityNode — recursive display via HierarchyComponent
    // ─────────────────────────────────────────────────────────────────────────

    void SceneHierarchyPanel::drawEntityNode(ECS::EntityID id)
    {
        auto& ctx = editor::EditorContext::get();

        const bool hasHierarchy = ctx.registry->hasComponent<ECS::HierarchyComponent>(id);
        const bool hasChildren  = hasHierarchy &&
            (ctx.registry->hasComponent<ECS::HierarchyComponent>(id) && ctx.registry->getComponent<ECS::HierarchyComponent>(id).firstChild != ECS::INVALID_VALUE);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (ctx.selection.getSelectedEntityID() == id)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const std::string label = getEntityLabel(id) + "##" + std::to_string(id);
        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::IsItemClicked())
            ctx.selection.select(id);

        drawEntityContextMenu(id);

        if (hasChildren && open)
        {
            if (ctx.registry->hasComponent<ECS::HierarchyComponent>(id)) {
                ECS::EntityID child = ctx.registry->getComponent<ECS::HierarchyComponent>(id).firstChild;
                while (child != ECS::INVALID_VALUE) {
                    drawEntityNode(child);
                    if (ctx.registry->hasComponent<ECS::HierarchyComponent>(child)) {
                        child = ctx.registry->getComponent<ECS::HierarchyComponent>(child).nextSibling;
                    } else {
                        break;
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::drawEntityContextMenu(ECS::EntityID id)
    {
        const std::string popupId = "ctx_entity_" + std::to_string(id);

        if (ImGui::BeginPopupContextItem())
        {
            auto& ctx  = editor::EditorContext::get();
            auto& scene = SLS::SceneManager::instance().current();

            if (ImGui::MenuItem((std::string(ICON_FA_PLUS) + "Add Child").c_str()))
            {
                const ECS::EntityID child = scene.createEntity("Entity");
                scene.setParent(child, id);
                ctx.selection.select(child);
            }
            ImGui::Separator();
            if (ImGui::MenuItem((std::string(ICON_FA_TRASH) +" Delete").c_str()))
            {
                ctx.selection.select(id);
                removeSelected();
            }
            ImGui::EndPopup();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Spawn menu
    // ─────────────────────────────────────────────────────────────────────────

    void SceneHierarchyPanel::drawSpawnMenu()
    {
        auto& ctx        = editor::EditorContext::get();
        auto& scene      = SLS::SceneManager::instance().current();
        const glm::vec3 frontOfCam = ctx.camera->m_position + ctx.camera->m_front * 4.0f;

        // ── Generic entity ────────────────────────────────────────────────────
        if (ImGui::Button((std::string(ICON_FA_PLUS) + " Add entity").c_str()))
            scene.createEntity("Entity");

        // ── Quick-spawn shapes / lights ───────────────────────────────────────
        if (ImGui::BeginMenu("Quick spawn"))
        {
            if (ImGui::BeginMenu("Shape"))
            {
                if (ImGui::MenuItem("Cone"))      editor::spawnCone     (scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Cube"))      editor::spawnCube     (scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Cylinder"))  editor::spawnCylinder (scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Icosphere")) editor::spawnIcosphere(scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Sphere"))    editor::spawnSphere   (scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Suzanne"))   editor::spawnSuzanne  (scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Torus"))     editor::spawnTorus    (scene, ctx.pRHI, frontOfCam);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Light"))
            {
                if (ImGui::MenuItem("Spot light"))  editor::spawnSpotLight (scene, ctx.pRHI, frontOfCam);
                if (ImGui::MenuItem("Point light")) editor::spawnPointLight(scene, ctx.pRHI, frontOfCam);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Prefab..."))
            {
                const char* filters[] = { "*.gcprefab" };
                const char* path = tinyfd_openFileDialog("Choose a prefab",
                    std::filesystem::current_path().string().c_str(), 1, filters, "GCEP Prefab", 0);
                if (path)
                {
                    const ECS::EntityID id = editor::PrefabSystem::instantiate(scene, ctx.pRHI, path, frontOfCam);
                    if (id != ECS::INVALID_VALUE)
                        ctx.selection.select(id);
                }
            }
            if (ImGui::MenuItem("Asset..."))
            {
                const char* filters[] = { "*.obj", "*.gltf" };
                const char* path = tinyfd_openFileDialog("Choose an asset",
                    std::filesystem::current_path().string().c_str(), 2, filters, "3D Object files", 0);
                if (path)
                    editor::spawnAsset(scene, ctx.pRHI, path, frontOfCam);
            }
            ImGui::EndMenu();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // removeSelected
    // ─────────────────────────────────────────────────────────────────────────

    void SceneHierarchyPanel::removeSelected()
    {
        auto& ctx   = editor::EditorContext::get();
        auto& scene = SLS::SceneManager::instance().current();
        const ECS::EntityID id = ctx.selection.getSelectedEntityID();

        auto* meshData   = ctx.pRHI->getInitInfos()->meshData;
        auto& spotLights = ctx.pRHI->getLightSystem().getSpotLights();
        auto& pointLights= ctx.pRHI->getLightSystem().getPointLights();

        const bool isMesh  = std::ranges::any_of(*meshData,    [id](const auto& m) { return m.id == id; });
        const bool isSpot  = std::ranges::any_of(spotLights,   [id](const auto& m) { return m.id == id; });
        const bool isPoint = std::ranges::any_of(pointLights,  [id](const auto& m) { return m.id == id; });

        if      (isMesh)  ctx.pRHI->removeMesh(id);
        else if (isSpot)  ctx.pRHI->getLightSystem().removeSpotLight(id);
        else if (isPoint) ctx.pRHI->getLightSystem().removePointLight(id);

        scene.destroyEntity(id);
        ctx.selection.unselect();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // draw
    // ─────────────────────────────────────────────────────────────────────────

    void SceneHierarchyPanel::draw()
    {
        auto& ctx = editor::EditorContext::get();

        ImGui::Begin("Scene Hierarchy");

        auto& scene = SLS::SceneManager::instance().current();
        if (ImGui::Button((std::string(ICON_FA_PLUS) + " Add Entity").c_str()))
            scene.createEntity("Entity");

        ImGui::SeparatorText("Scene tree");

        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto isRoot = [&](ECS::EntityID id)
            {
                if (!ctx.registry->hasComponent<ECS::HierarchyComponent>(id))
                    return true;
                return ctx.registry->getComponent<ECS::HierarchyComponent>(id).parent == ECS::INVALID_VALUE;
            };

            // Iterate all entities that have a Tag (i.e. all scene entities)
            auto tagView = ctx.registry->view<ECS::Tag>();
            for (const ECS::EntityID id : tagView)
            {
                if (isRoot(id))
                    drawEntityNode(id);
            }

            ImGui::TreePop();
        }

        // Clic droit sur fond vide
        if (ImGui::BeginPopupContextWindow("ctx_hierarchy_bg",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem((std::string(ICON_FA_PLUS) + " Add Entity").c_str()))
                scene.createEntity("Entity");
            ImGui::EndPopup();
        }

        // Delete shortcut
        if (ctx.selection.hasSelectedEntity() && ImGui::IsKeyPressed(ImGuiKey_Delete))
            removeSelected();

        // Deselect on empty click (not when clicking an item)
        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
            ctx.selection.unselect();

        ImGui::End();
    }

} // namespace gcep::panel
