#include "scene_settings_panel.hpp"

#include <Editor/ProjectLoader/project_loader.hpp>
#include <Scene/header/scene_manager.hpp>
#include <imgui.h>

namespace gcep::panel
{
    void SceneSettingsPanel::draw()
    {
        auto& ctx      = editor::EditorContext::get();
        auto& settings = SLS::SceneManager::instance().current().getSceneSettings();
        auto& ps       = pl::ProjectLoader::instance().getSettings();

        ImGui::Begin("Settings");

        // ── Rendu ─────────────────────────────────────────────────────────────
        bool vsync = ctx.pRHI->isVSync();
        if (ImGui::Checkbox("V-Sync", &vsync))
        {
            ctx.pRHI->setVSync(!ctx.pRHI->isVSync());
            ps.vsync = ctx.pRHI->isVSync();
            pl::ProjectLoader::instance().markDirty();
        }

        ImGui::SeparatorText("Scene infos");
        if (ImGui::ColorEdit4("Clear color", ps.clearColor,
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel))
        {
            settings.clearColor = { ps.clearColor[0], ps.clearColor[1],
                                    ps.clearColor[2], ps.clearColor[3] };
            pl::ProjectLoader::instance().markDirty();
        }
        ImGui::Text("Total entities : %d", (int)ctx.pRHI->getInitInfos()->meshData->size());
        ImGui::Text("Entities drawn : %d", ctx.pRHI->getDrawCount());

        // ── Camera ────────────────────────────────────────────────────────────
        ImGui::SeparatorText("Camera");
        if (ImGui::SliderFloat("Camera Speed", &ps.cameraSpeed, 1.0f, 20.0f))
        {
            ctx.camSpeed = ps.cameraSpeed;
            pl::ProjectLoader::instance().markDirty();
        }

        // ── Lumière ───────────────────────────────────────────────────────────
        ImGui::SeparatorText("Scene light");
        if (ImGui::ColorEdit3("Ambient color", ps.ambientColor,
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel))
        {
            settings.ambientColor = { ps.ambientColor[0], ps.ambientColor[1], ps.ambientColor[2] };
            pl::ProjectLoader::instance().markDirty();
        }
        if (ImGui::ColorEdit3("Light color", ps.lightColor,
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel))
        {
            settings.lightColor = { ps.lightColor[0], ps.lightColor[1], ps.lightColor[2] };
            pl::ProjectLoader::instance().markDirty();
        }
        if (ImGui::DragFloat3("Light direction", ps.lightDirection))
        {
            settings.lightDirection = { ps.lightDirection[0], ps.lightDirection[1], ps.lightDirection[2] };
            pl::ProjectLoader::instance().markDirty();
        }

        // ── Grille ────────────────────────────────────────────────────────────
        ImGui::SeparatorText("Grid options");
        if (ImGui::InputFloat("Cell size",     &ps.gridCellSize))     { ctx.sceneInfos.cellSize     = ps.gridCellSize;     pl::ProjectLoader::instance().markDirty(); }
        if (ImGui::InputFloat("Thick every",   &ps.gridThickEvery))   { ctx.sceneInfos.thickEvery   = ps.gridThickEvery;   pl::ProjectLoader::instance().markDirty(); }
        if (ImGui::InputFloat("Fade distance", &ps.gridFadeDistance)) { ctx.sceneInfos.fadeDistance = ps.gridFadeDistance; pl::ProjectLoader::instance().markDirty(); }
        if (ImGui::InputFloat("Line width",    &ps.gridLineWidth))    { ctx.sceneInfos.lineWidth    = ps.gridLineWidth;    pl::ProjectLoader::instance().markDirty(); }

        // ── TAA ───────────────────────────────────────────────────────────────
        ImGui::SeparatorText("Temporal Anti-Aliasing");
        if (ImGui::InputFloat("Blend alpha", &ps.taaBlendAlpha, 0.01f, 0.10f))
        {
            ctx.pRHI->setTAABlendAlpha(ps.taaBlendAlpha);
            pl::ProjectLoader::instance().markDirty();
        }

        // ── Infos ─────────────────────────────────────────────────────────────
        ImGui::SeparatorText("Application infos");
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Viewport : %.0f x %.0f", ctx.viewportSize.x, ctx.viewportSize.y);

        ImGui::End();
    }

} // namespace gcep::panel
