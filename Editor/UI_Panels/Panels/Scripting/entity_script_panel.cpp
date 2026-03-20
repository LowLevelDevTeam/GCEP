#include "entity_script_panel.hpp"

// Internals
#include <config.hpp>
#include <Editor/UI_Panels/editor_context.hpp>

// Externals
#include <font_awesome.hpp>
#include <imgui.h>

// STL
#include <filesystem>
#include <fstream>
#include <iostream>

namespace gcep::panel
{
    void EntityScriptPanel::drawEntityScriptSection()
    {
        auto& ctx = editor::EditorContext::get();
        if (!m_state.manager || !ctx.selection.hasSelectedEntity()) return;

        const ECS::EntityID    entityId  = ctx.selection.getSelectedEntityID();
        const bool             simPlaying = (ctx.simulationState == SimulationState::PLAYING);
        const ImGuiIO&         io         = ImGui::GetIO();

        auto it = m_state.entityScripts.find(entityId);

        if (it != m_state.entityScripts.end())
        {
            AttachedScript& entry  = it->second;
            LoadedScript*   script = m_state.manager->getScript(entry.scriptName);
            const bool      valid  = script && script->valid;

            ImGui::TextColored(
                valid ? ImVec4{0.2f, 0.9f, 0.2f, 1.f} : ImVec4{0.9f, 0.3f, 0.2f, 1.f},
                valid ? ICON_FA_CHECK_CIRCLE : ICON_FA_EXCLAMATION_TRIANGLE);
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.scriptName.c_str());

            if (!simPlaying)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton((std::string(ICON_FA_REFRESH) + "##reload").c_str()))
                {
                    reloadScriptOnEntity(entityId, entry);
                    m_state.setStatus("Reloaded: " + entry.scriptName);
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.65f, 0.1f, 0.1f, 1.f});
                if (ImGui::SmallButton((std::string(ICON_FA_TIMES) + "##remove").c_str()))
                {
                    destroyInstance(entry);
                    m_state.entityScripts.erase(it);
                    ImGui::PopStyleColor();
                    return;
                }
                ImGui::PopStyleColor();
            }
        }
        else
        {
            ImGui::TextDisabled("No script attached.");
        }

        ImGui::Spacing();

        if (!simPlaying && it == m_state.entityScripts.end())
        {
            if (ImGui::Button((std::string(ICON_FA_CODE) + " Add script").c_str()))
                createAndAttach(entityId);

            auto names = m_state.manager->getAvailableScriptNames();
            if (!names.empty())
            {
                ImGui::SameLine();
                auto& preview = m_state.attachCombo[entityId];
                if (preview.empty()) preview = names[0];

                ImGui::SetNextItemWidth(150.f);
                const std::string comboId = "##attachcombo_" + std::to_string(entityId);
                if (ImGui::BeginCombo(comboId.c_str(), preview.c_str()))
                {
                    for (auto& n : names)
                    {
                        bool sel = (preview == n);
                        if (ImGui::Selectable(n.c_str(), sel)) preview = n;
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button((std::string(ICON_FA_PLUS) + " Attach").c_str()))
                    attachExisting(entityId, preview);
            }
        }

        if (m_state.statusTimer > 0.f)
        {
            ImGui::TextColored({0.4f, 0.8f, 1.f, 1.f}, "%s", m_state.statusMsg.c_str());
            m_state.statusTimer -= io.DeltaTime;
        }
    }

    void EntityScriptPanel::createAndAttach(ECS::EntityID entityId)
    {
        if (m_state.entityScripts.count(entityId))
        {
            m_state.setStatus("Entity already has a script.");
            return;
        }

        const std::string scriptName = "Script_" + std::to_string(entityId);
        const std::string outputPath = m_state.scriptsDir + "/" + scriptName + ".cpp";
        const std::string modelPath  = std::string(PROJECT_ROOT) + "/Engine/Core/Scripting/API/script_model.cpp";

        std::filesystem::create_directories(m_state.scriptsDir);

        if (std::filesystem::exists(outputPath))
        {
            m_state.setStatus("File exists, attaching: " + scriptName);
            registerEntry(entityId, scriptName);
            return;
        }

        std::ifstream in(modelPath, std::ios::binary);
        if (!in.is_open())
        {
            Log::error(std::format("Cannot open script model: {}", modelPath));
            m_state.setStatus("ERROR: model not found.");
            return;
        }

        std::string src((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        for (std::size_t p = 0; (p = src.find("SCRIPT_NAME", p)) != std::string::npos;)
        {
            src.replace(p, 11u, scriptName);
            p += scriptName.size();
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open())
        {
            Log::error(std::format("Cannot write: {}", outputPath));
            m_state.setStatus("ERROR: write failed.");
            return;
        }
        out << src;
        out.close();

        Log::info(std::format("Created: {}", outputPath));
        m_state.manager->pollForChanges();
        registerEntry(entityId, scriptName);
        m_state.setStatus("Created & attached: " + scriptName);
    }

    void EntityScriptPanel::attachExisting(ECS::EntityID entityId, const std::string& scriptName)
    {
        if (m_state.entityScripts.count(entityId))
        {
            m_state.setStatus("Entity already has a script.");
            return;
        }
        registerEntry(entityId, scriptName);
        m_state.setStatus("Attached: " + scriptName);
    }

    void EntityScriptPanel::registerEntry(ECS::EntityID entityId, const std::string& scriptName)
    {
        auto& component      = m_state.registry->addComponent<ECS::ScriptComponent>(entityId);
        component.entityId   = entityId;
        component.scriptName = scriptName;

        AttachedScript entry;
        entry.scriptName      = scriptName;
        entry.comp.scriptName = scriptName;
        entry.comp.entityId   = static_cast<unsigned int>(entityId);
        entry.runtime.started = false;

        if (m_state.manager->getScript(scriptName))
            m_state.manager->createInstance(entry.comp, entry.runtime, m_state.registry);

        m_state.entityScripts.emplace(entityId, std::move(entry));
    }

    void EntityScriptPanel::destroyInstance(AttachedScript& entry)
    {
        if (m_state.manager)
            m_state.manager->destroyInstance(entry.comp, entry.runtime, m_state.registry);
    }

    void EntityScriptPanel::reloadScriptOnEntity(ECS::EntityID eid, AttachedScript& entry)
    {
        if (!m_state.manager) return;
        ScriptContext ctx = m_state.makeCtx(eid);
        m_state.manager->callOnEnd(entry.comp, entry.runtime, ctx);
        m_state.manager->destroyInstance(entry.comp, entry.runtime, m_state.registry);
        entry.runtime.started = false;
        m_state.manager->pollForChanges();
        m_state.manager->createInstance(entry.comp, entry.runtime, m_state.registry);
        m_state.manager->callOnStart(entry.comp, entry.runtime, ctx);
    }

} // namespace gcep::panel
