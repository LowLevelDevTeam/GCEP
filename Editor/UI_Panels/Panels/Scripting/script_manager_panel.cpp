#include "script_manager_panel.hpp"

// Externals
#include <font_awesome.hpp>
#include <imgui.h>

// STL
#include <filesystem>
#include <fstream>

namespace gcep::panel
{
    void ScriptManagerPanel::draw()
    {
        if (!m_state.manager) return;

        if (!ImGui::Begin("Script Manager", nullptr, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        ImGui::PushFont(io.Fonts->Fonts[0]);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::MenuItem((std::string(ICON_FA_REFRESH) + " Reload all").c_str()))
            {
                reloadAllScripts();
                m_state.setStatus("All scripts reloaded.");
            }
            if (ImGui::MenuItem((std::string(ICON_FA_SEARCH) + " Poll changes").c_str()))
            {
                m_state.manager->pollForChanges();
                m_state.setStatus("Polled.");
            }
            if (ImGui::MenuItem((std::string(ICON_FA_PLAY) + " Compile").c_str()))
                compileScripts();
            if (ImGui::MenuItem((std::string(ICON_FA_FOLDER_OPEN) + " Open dir").c_str()))
                openScriptsDir();
            ImGui::EndMenuBar();
        }

        ImGui::SeparatorText("Loaded scripts");

        auto names = m_state.manager->getAvailableScriptNames();
        if (names.empty())
        {
            ImGui::TextDisabled("No scripts compiled yet.");
            if (!m_state.scriptsDir.empty())
            {
                ImGui::TextDisabled("Watching:");
                ImGui::TextDisabled("%s", m_state.scriptsDir.c_str());
            }
        }
        else if (ImGui::BeginTable("##smtable", 3,
                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                     ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
                     ImVec2(0.f, 180.f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("State",   ImGuiTableColumnFlags_WidthFixed, 56.f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100.f);
            ImGui::TableHeadersRow();

            for (auto& name : names)
            {
                LoadedScript* s = m_state.manager->getScript(name);
                ImGui::TableNextRow();
                ImGui::PushID(name.c_str());

                ImGui::TableSetColumnIndex(0);
                bool sel = (m_state.selectedScript == name);

                if (m_state.renamingIndex < 0 && sel &&
                    ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered())
                {
                    m_state.renamingIndex = 0;
                    strncpy(m_state.renameBuffer, name.c_str(), sizeof(m_state.renameBuffer) - 1);
                    m_state.renameBuffer[sizeof(m_state.renameBuffer) - 1] = '\0';
                }

                if (m_state.renamingIndex >= 0 && sel)
                {
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##rename", m_state.renameBuffer,
                            sizeof(m_state.renameBuffer),
                            ImGuiInputTextFlags_EnterReturnsTrue |
                            ImGuiInputTextFlags_EscapeClearsAll))
                    {
                        renameScript(name, std::string(m_state.renameBuffer));
                        m_state.renamingIndex = -1;
                    }
                    if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                        m_state.renamingIndex = -1;
                }
                else
                {
                    if (ImGui::Selectable(name.c_str(), sel,
                            ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowOverlap))
                        m_state.selectedScript = name;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Double-click to rename");
                }

                ImGui::TableSetColumnIndex(1);
                if (s && s->valid)
                    ImGui::TextColored({0.2f, 0.9f, 0.2f, 1.f}, (std::string(ICON_FA_CHECK) + " OK").c_str());
                else
                    ImGui::TextColored({0.9f, 0.3f, 0.2f, 1.f}, (std::string(ICON_FA_TIMES) + " ERR").c_str());

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton((std::string(ICON_FA_REFRESH) + " Reload").c_str()))
                {
                    reloadAllScripts(name);
                    m_state.setStatus("Reloaded: " + name);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_INFO))
                    m_state.selectedScript = name;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (!m_state.selectedScript.empty())
        {
            ImGui::SeparatorText(m_state.selectedScript.c_str());
            LoadedScript* s = m_state.manager->getScript(m_state.selectedScript);
            if (s && s->valid)
            {
                ImGui::LabelText("Source",  "%s", s->sourcePath.c_str());
                ImGui::LabelText("Library", "%s", s->libPath.c_str());

                int liveCount = 0;
                for (auto& [eid, entry] : m_state.entityScripts)
                    if (entry.scriptName == m_state.selectedScript && entry.runtime.instance)
                        ++liveCount;
                ImGui::LabelText("Live instances", "%d", liveCount);
            }
            else
            {
                ImGui::TextColored({0.9f, 0.3f, 0.2f, 1.f},
                    (std::string(ICON_FA_EXCLAMATION_TRIANGLE) + " Failed to compile / load.").c_str());
            }
        }

        ImGui::SeparatorText("Scene entities");

        if (m_state.entityScripts.empty())
        {
            ImGui::TextDisabled("No scripts attached to any entity.");
        }
        else if (ImGui::BeginTable("##smentities", 4,
                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                     ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
                     ImVec2(0.f, 160.f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Entity",  ImGuiTableColumnFlags_WidthFixed,  50.f);
            ImGui::TableSetupColumn("Script",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("State",   ImGuiTableColumnFlags_WidthFixed,  60.f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,  70.f);
            ImGui::TableHeadersRow();

            for (auto it = m_state.entityScripts.begin(); it != m_state.entityScripts.end();)
            {
                auto& [eid, entry] = *it;
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(eid));

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", static_cast<unsigned>(eid));

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.scriptName.c_str());

                ImGui::TableSetColumnIndex(2);
                if (!entry.runtime.instance)
                    ImGui::TextColored({0.5f, 0.5f, 0.5f, 1.f}, "None");
                else if (!entry.runtime.started)
                    ImGui::TextColored({0.9f, 0.8f, 0.1f, 1.f}, "Pending");
                else
                    ImGui::TextColored({0.2f, 0.9f, 0.2f, 1.f}, "Running");

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton((std::string(ICON_FA_REFRESH) + "##r").c_str()))
                {
                    // Reload inline via reloadAllScripts single-entry filter
                    reloadAllScripts(entry.scriptName);
                    m_state.setStatus("Reloaded on " + std::to_string(eid));
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.65f, 0.1f, 0.1f, 1.f});
                if (ImGui::SmallButton((std::string(ICON_FA_TIMES) + "##d").c_str()))
                {
                    m_state.manager->destroyInstance(entry.comp, entry.runtime, m_state.registry);
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                    it = m_state.entityScripts.erase(it);
                    continue;
                }
                ImGui::PopStyleColor();

                ImGui::PopID();
                ++it;
            }
            ImGui::EndTable();
        }

        if (m_state.statusTimer > 0.f)
        {
            ImGui::Separator();
            ImGui::TextColored({0.4f, 0.8f, 1.f, 1.f}, "%s", m_state.statusMsg.c_str());
            m_state.statusTimer -= io.DeltaTime;
        }

        ImGui::PopFont();
        ImGui::End();
    }

    void ScriptManagerPanel::onSimulationStart()
    {
        if (!m_state.manager) return;
        for (auto& [eid, entry] : m_state.entityScripts)
        {
            ScriptContext ctx = m_state.makeCtx(eid);
            m_state.manager->callOnStart(entry.comp, entry.runtime, ctx);
        }
    }

    void ScriptManagerPanel::onSimulationUpdate(float dt)
    {
        if (!m_state.manager) return;
        m_state.deltaTime = dt;
        for (auto& [eid, entry] : m_state.entityScripts)
        {
            ScriptContext ctx = m_state.makeCtx(eid);
            m_state.manager->callOnUpdate(entry.comp, entry.runtime, ctx);
        }
    }

    void ScriptManagerPanel::onSimulationStop()
    {
        if (!m_state.manager) return;
        for (auto& [eid, entry] : m_state.entityScripts)
        {
            ScriptContext ctx = m_state.makeCtx(eid);
            m_state.manager->callOnEnd(entry.comp, entry.runtime, ctx);
        }
    }

    void ScriptManagerPanel::reloadAllScripts(const std::string& filter)
    {
        if (!m_state.manager) return;
        for (auto& name : m_state.manager->getAvailableScriptNames())
        {
            if (!filter.empty() && name != filter) continue;
            for (auto& [eid, entry] : m_state.entityScripts)
                if (entry.scriptName == name && entry.runtime.instance)
                {
                    ScriptContext ctx = m_state.makeCtx(eid);
                    m_state.manager->callOnEnd(entry.comp, entry.runtime, ctx);
                    m_state.manager->destroyInstance(entry.comp, entry.runtime, m_state.registry);
                }
            m_state.manager->pollForChanges();
            for (auto& [eid, entry] : m_state.entityScripts)
                if (entry.scriptName == name)
                {
                    entry.runtime.started = false;
                    m_state.manager->createInstance(entry.comp, entry.runtime, m_state.registry);
                    ScriptContext ctx = m_state.makeCtx(eid);
                    m_state.manager->callOnStart(entry.comp, entry.runtime, ctx);
                }
        }
    }

    void ScriptManagerPanel::renameScript(const std::string& oldName, const std::string& newName)
    {
        if (newName.empty() || newName == oldName) return;

        const std::string oldPath = m_state.scriptsDir + "/" + oldName + ".cpp";
        const std::string newPath = m_state.scriptsDir + "/" + newName + ".cpp";

        if (std::filesystem::exists(newPath))
        {
            m_state.setStatus("ERROR: " + newName + " already exists.");
            return;
        }

        std::error_code ec;
        std::filesystem::rename(oldPath, newPath, ec);
        if (ec)
        {
            m_state.setStatus("ERROR: rename failed: " + ec.message());
            return;
        }

        std::ifstream in(newPath);
        std::string   src((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
        in.close();
        for (std::size_t p = 0; (p = src.find(oldName, p)) != std::string::npos;)
        {
            src.replace(p, oldName.size(), newName);
            p += newName.size();
        }
        std::ofstream out(newPath);
        out << src;
        out.close();

        for (auto& [eid, entry] : m_state.entityScripts)
            if (entry.scriptName == oldName)
            {
                entry.scriptName      = newName;
                entry.comp.scriptName = newName;
            }

        if (m_state.selectedScript == oldName)
            m_state.selectedScript = newName;

        m_state.manager->pollForChanges();
        m_state.setStatus("Renamed: " + oldName + " -> " + newName);
    }

    void ScriptManagerPanel::compileScripts()
    {
        if (m_state.cmakeBuildDir.empty())
        {
            m_state.setStatus("ERROR: cmake build dir not set.");
            return;
        }
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "cmake",
                ("--build \"" + m_state.cmakeBuildDir + "\" --target GCEngineScripts").c_str(),
                nullptr, SW_HIDE);
        #else
            std::system(("cmake --build \"" + m_state.cmakeBuildDir + "\" --target GCEngineScripts &").c_str());
        #endif
        m_state.setStatus("Compiling scripts...");
    }

    void ScriptManagerPanel::openScriptsDir()
    {
        std::string absDir = std::filesystem::absolute(m_state.scriptsDir).string();
        std::filesystem::create_directories(absDir);
        #ifdef _WIN32
            ShellExecuteA(nullptr, "explore", absDir.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        #elif defined(__APPLE__)
            std::system(("open \"" + absDir + "\"").c_str());
        #else
            std::system(("xdg-open \"" + absDir + "\"").c_str());
        #endif
    }

} // namespace gcep::panel
