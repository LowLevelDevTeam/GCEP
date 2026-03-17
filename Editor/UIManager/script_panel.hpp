#pragma once

/// @file script_panel.hpp
/// @brief Editor scripting panel.
///
/// Two rendering surfaces:
///   - drawEntityScriptSection()  inline section in "Entity properties" (showMeshInfos),
///                                placed between Physics and Gizmo controls.
///   - drawManagerWindow()        always-visible docked "Script Manager" tab,
///                                called unconditionally from uiUpdate().
///
/// UiManager owns both a ScriptHotReloadManager and a ScriptPanel as value members.
/// The constructor calls m_scriptManager.init(...) then m_scriptPanel.setManager(&m_scriptManager, registry).
/// Nothing else needs to touch these objects from outside.

// Internals
#include <Engine/Core/Scripting/script_hot_reload.hpp>
#include <ECS/Components/components.hpp>
#include <Editor/Helpers.hpp>
#include <config.hpp>

// Externals
#include <font_awesome.hpp>
#include <imgui.h>

// STL
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace gcep
{
    /// @class ScriptPanel
    /// @brief Self-contained ImGui panel for the hot-reload scripting system.
    ///
    /// Holds a non-owning pointer to a ScriptHotReloadManager (owned by UiManager)
    /// and tracks which scripts are attached to which entities.
    class ScriptPanel
    {
    public:

        // Setup

        void setManager(ScriptHotReloadManager* manager, ECS::Registry* registry)
        {
            m_manager  = manager;
            m_registry = registry;
        }

        void setRegistry    (ECS::Registry* registry) { m_registry     = registry;  }
        void setScriptsDir  (const std::string& dir)   { m_scriptsDir   = dir;       }
        void setBuildDir    (const std::string& dir)   { m_cmakeBuildDir = dir;      }

        // Entity properties inline section
        // Call from showMeshInfos(), between Physics and Gizmo separators.

        void drawEntityScriptSection(ECS::EntityID entityId, SimulationState simState)
        {
            if (!m_manager) return;

            const bool     simPlaying = (simState == SimulationState::PLAYING);
            const ImGuiIO& io         = ImGui::GetIO();

            ImGui::SeparatorText("Scripting");
            ImGui::PushFont(io.Fonts->Fonts[0]);

            auto it = m_entityScripts.find(entityId);

            // ── Attached script ────────────────────────────────────────────────────
            if (it != m_entityScripts.end())
            {
                AttachedScript& entry = it->second;
                LoadedScript*   script = m_manager->getScript(entry.scriptName);
                bool            valid  = script && script->valid;

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
                        setStatus("Reloaded: " + entry.scriptName);
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.65f, 0.1f, 0.1f, 1.f});
                    if (ImGui::SmallButton((std::string(ICON_FA_TIMES) + "##remove").c_str()))
                    {
                        destroyInstance(entry);
                        m_entityScripts.erase(it);
                        ImGui::PopStyleColor();
                        ImGui::PopFont();
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

            // ── Add / attach controls — only shown when no script is attached ──────
            if (!simPlaying && it == m_entityScripts.end())
            {
                if (ImGui::Button((std::string(ICON_FA_CODE) + " Add script").c_str()))
                    createAndAttach(entityId);

                auto names = m_manager->getAvailableScriptNames();
                if (!names.empty())
                {
                    ImGui::SameLine();
                    auto& preview = m_attachCombo[entityId];
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

            if (m_statusTimer > 0.f)
            {
                ImGui::TextColored({0.4f, 0.8f, 1.f, 1.f}, "%s", m_statusMsg.c_str());
                m_statusTimer -= io.DeltaTime;
            }

            ImGui::PopFont();
        }

        // ── Docked "Script Manager" window ────────────────────────────────────────
        // Title must match initDockspace registration exactly.
        // Called unconditionally from uiUpdate() every frame.

        void drawManagerWindow()
        {
            if (!m_manager) return;

            if (!ImGui::Begin("Script Manager", nullptr, ImGuiWindowFlags_MenuBar))
            {
                ImGui::End();
                return;
            }

            const ImGuiIO& io = ImGui::GetIO();
            ImGui::PushFont(io.Fonts->Fonts[0]);

            // ── Menu bar ──────────────────────────────────────────────────────────
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::MenuItem((std::string(ICON_FA_REFRESH) + " Reload all").c_str()))
                {
                    reloadAllScripts();
                    setStatus("All scripts reloaded.");
                }
                if (ImGui::MenuItem((std::string(ICON_FA_SEARCH) + " Poll changes").c_str()))
                {
                    m_manager->pollForChanges();
                    setStatus("Polled.");
                }
                if (ImGui::MenuItem((std::string(ICON_FA_PLAY) + " Compile").c_str()))
                    compileScripts();
                if (ImGui::MenuItem((std::string(ICON_FA_FOLDER_OPEN) + " Open dir").c_str()))
                    openScriptsDir();
                ImGui::EndMenuBar();
            }

            // ── Loaded scripts table ──────────────────────────────────────────────
            ImGui::SeparatorText("Loaded scripts");

            auto names = m_manager->getAvailableScriptNames();
            if (names.empty())
            {
                ImGui::TextDisabled("No scripts compiled yet.");
                if (!m_scriptsDir.empty())
                {
                    ImGui::TextDisabled("Watching:");
                    ImGui::TextDisabled("%s", m_scriptsDir.c_str());
                }
            }
            else
            {
                if (ImGui::BeginTable("##smtable", 3,
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
                        LoadedScript* s = m_manager->getScript(name);
                        ImGui::TableNextRow();
                        ImGui::PushID(name.c_str());

                        ImGui::TableSetColumnIndex(0);
                        bool sel = (m_selectedScript == name);

                        // Inline rename: double-click the name to edit it
                        if (m_renamingIndex < 0 &&
                            sel && ImGui::IsMouseDoubleClicked(0) &&
                            ImGui::IsItemHovered())
                        {
                            m_renamingIndex = 0;
                            strncpy(m_renameBuffer, name.c_str(), sizeof(m_renameBuffer) - 1);
                            m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
                        }

                        if (m_renamingIndex >= 0 && sel)
                        {
                            ImGui::SetNextItemWidth(-1.f);
                            ImGui::SetKeyboardFocusHere();
                            if (ImGui::InputText("##rename", m_renameBuffer,
                                    sizeof(m_renameBuffer),
                                    ImGuiInputTextFlags_EnterReturnsTrue |
                                    ImGuiInputTextFlags_EscapeClearsAll))
                            {
                                renameScript(name, std::string(m_renameBuffer));
                                m_renamingIndex = -1;
                            }
                            if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                                m_renamingIndex = -1;
                        }
                        else
                        {
                            if (ImGui::Selectable(name.c_str(), sel,
                                    ImGuiSelectableFlags_SpanAllColumns |
                                    ImGuiSelectableFlags_AllowOverlap))
                                m_selectedScript = name;
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
                            setStatus("Reloaded: " + name);
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton(ICON_FA_INFO))
                            m_selectedScript = name;

                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }

            // ── Selected script detail pane ───────────────────────────────────────
            if (!m_selectedScript.empty())
            {
                ImGui::SeparatorText(m_selectedScript.c_str());
                LoadedScript* s = m_manager->getScript(m_selectedScript);
                if (s && s->valid)
                {
                    ImGui::LabelText("Source",  "%s", s->sourcePath.c_str());
                    ImGui::LabelText("Library", "%s", s->libPath.c_str());

                    int liveCount = 0;
                    for (auto& [eid, entry] : m_entityScripts)
                        if (entry.scriptName == m_selectedScript && entry.comp.instance)
                            ++liveCount;
                    ImGui::LabelText("Live instances", "%d", liveCount);
                }
                else
                {
                    ImGui::TextColored({0.9f, 0.3f, 0.2f, 1.f}, (std::string(ICON_FA_EXCLAMATION_TRIANGLE) + " Failed to compile / load.").c_str());
                }
            }

            // ── Scene entities table ──────────────────────────────────────────────
            ImGui::SeparatorText("Scene entities");

            int totalAttached = static_cast<int>(m_entityScripts.size());

            if (totalAttached == 0)
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

                for (auto it = m_entityScripts.begin(); it != m_entityScripts.end();)
                {
                    auto& [eid, entry] = *it;
                    ImGui::TableNextRow();
                    ImGui::PushID(static_cast<int>(eid));

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%u", static_cast<unsigned>(eid));

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(entry.scriptName.c_str());

                    ImGui::TableSetColumnIndex(2);
                    if (!entry.comp.instance)
                        ImGui::TextColored({0.5f, 0.5f, 0.5f, 1.f}, "None");
                    else if (!entry.comp.started)
                        ImGui::TextColored({0.9f, 0.8f, 0.1f, 1.f}, "Pending");
                    else
                        ImGui::TextColored({0.2f, 0.9f, 0.2f, 1.f}, "Running");

                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::SmallButton((std::string(ICON_FA_REFRESH) + "##r").c_str()))
                    {
                        reloadScriptOnEntity(eid, entry);
                        setStatus("Reloaded on " + std::to_string(eid));
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.65f, 0.1f, 0.1f, 1.f});
                    if (ImGui::SmallButton((std::string(ICON_FA_TIMES) + "##d").c_str()))
                    {
                        destroyInstance(entry);
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                        it = m_entityScripts.erase(it);
                        continue;
                    }
                    ImGui::PopStyleColor();

                    ImGui::PopID();
                    ++it;
                }
                ImGui::EndTable();
            }

            // ── Status toast ──────────────────────────────────────────────────────
            if (m_statusTimer > 0.f)
            {
                ImGui::Separator();
                ImGui::TextColored({0.4f, 0.8f, 1.f, 1.f}, "%s", m_statusMsg.c_str());
                m_statusTimer -= io.DeltaTime;
            }

            ImGui::PopFont();
            ImGui::End();
        }

        // ── Simulation lifecycle ──────────────────────────────────────────────────

        void onSimulationStart()
        {
            if (!m_manager) return;
            for (auto& [eid, entry] : m_entityScripts)
            {
                ScriptContext ctx = makeCtx(eid);
                m_manager->callOnStart(entry.comp, ctx);
            }
        }

        void onSimulationUpdate(float dt)
        {
            if (!m_manager) return;
            m_deltaTime = dt;
            for (auto& [eid, entry] : m_entityScripts)
            {
                ScriptContext ctx = makeCtx(eid);
                m_manager->callOnUpdate(entry.comp, ctx);
            }
        }

        void onSimulationStop()
        {
            if (!m_manager) return;
            for (auto& [eid, entry] : m_entityScripts)
            {
                ScriptContext ctx = makeCtx(eid);
                m_manager->callOnEnd(entry.comp, ctx);
            }
        }

    private:

        struct AttachedScript
        {
            std::string     scriptName;
            ScriptComponent comp;
        };

        ScriptContext makeCtx(ECS::EntityID eid) const
        {
            return m_manager->makeContext(
                static_cast<unsigned int>(eid),
                m_deltaTime,
                m_registry);
        }

        void createAndAttach(ECS::EntityID entityId)
        {
            // One script per entity — bail if one is already attached.
            if (m_entityScripts.count(entityId))
            {
                setStatus("Entity already has a script.");
                return;
            }

            const std::string scriptName = "Script_" + std::to_string(entityId);
            const std::string outputPath = m_scriptsDir + "/" + scriptName + ".cpp";
            const std::string modelPath  = std::string(PROJECT_ROOT)
                                         + "/Engine/Core/Scripting/script_model.cpp";

            std::filesystem::create_directories(m_scriptsDir);

            if (std::filesystem::exists(outputPath))
            {
                setStatus("File exists, attaching: " + scriptName);
                registerEntry(entityId, scriptName);
                return;
            }

            std::ifstream in(modelPath, std::ios::binary);
            if (!in.is_open())
            {
                std::cerr << "[ScriptPanel] Cannot open model: " << modelPath << "\n";
                setStatus("ERROR: model not found.");
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
                std::cerr << "[ScriptPanel] Cannot write: " << outputPath << "\n";
                setStatus("ERROR: write failed.");
                return;
            }
            out << src;
            out.close();

            std::cout << "[ScriptPanel] Created: " << outputPath << "\n";
            m_manager->pollForChanges();
            registerEntry(entityId, scriptName);
            setStatus("Created & attached: " + scriptName);
        }

        void attachExisting(ECS::EntityID entityId, const std::string& scriptName)
        {
            if (m_entityScripts.count(entityId))
            {
                setStatus("Entity already has a script.");
                return;
            }
            registerEntry(entityId, scriptName);
            setStatus("Attached: " + scriptName);
        }

        void registerEntry(ECS::EntityID entityId, const std::string& scriptName)
        {
            auto& component = m_registry->addComponent<ScriptComponent>(entityId);
            component.entityId   = entityId;
            component.scriptName = scriptName;
            component.started    = false;

            AttachedScript entry;
            entry.scriptName      = scriptName;
            entry.comp.scriptName = scriptName;
            entry.comp.entityId   = static_cast<unsigned int>(entityId);

            if (m_manager->getScript(scriptName))
                m_manager->createInstance(entry.comp, m_registry);

            m_entityScripts.emplace(entityId, std::move(entry));
        }

        void destroyInstance(AttachedScript& entry)
        {
            if (m_manager) m_manager->destroyInstance(entry.comp, m_registry);
        }

        void reloadScriptOnEntity(ECS::EntityID eid, AttachedScript& entry)
        {
            if (!m_manager) return;
            ScriptContext ctx = makeCtx(eid);
            m_manager->callOnEnd(entry.comp, ctx);
            m_manager->destroyInstance(entry.comp, m_registry);
            entry.comp.started = false;
            m_manager->pollForChanges();
            m_manager->createInstance(entry.comp, m_registry);
            m_manager->callOnStart(entry.comp, ctx);
        }

        void reloadAllScripts(const std::string& filter = "")
        {
            if (!m_manager) return;
            for (auto& name : m_manager->getAvailableScriptNames())
            {
                if (!filter.empty() && name != filter) continue;
                for (auto& [eid, entry] : m_entityScripts)
                    if (entry.scriptName == name && entry.comp.instance)
                    {
                        ScriptContext ctx = makeCtx(eid);
                        m_manager->callOnEnd(entry.comp, ctx);
                        m_manager->destroyInstance(entry.comp, m_registry);
                    }
                m_manager->pollForChanges();
                for (auto& [eid, entry] : m_entityScripts)
                    if (entry.scriptName == name)
                    {
                        entry.comp.started = false;
                        m_manager->createInstance(entry.comp, m_registry);
                        ScriptContext ctx = makeCtx(eid);
                        m_manager->callOnStart(entry.comp, ctx);
                    }
            }
        }

        void compileScripts()
        {
            if (m_cmakeBuildDir.empty())
            {
                setStatus("ERROR: cmake build dir not set.");
                return;
            }
            #ifdef _WIN32
                ShellExecuteA(nullptr, "open", "cmake",
                    ("--build \"" + m_cmakeBuildDir + "\" --target GCEngineScripts").c_str(),
                    nullptr, SW_HIDE);
            #else
                std::system(("cmake --build \"" + m_cmakeBuildDir + "\" --target GCEngineScripts &").c_str());
            #endif
            setStatus("Compiling scripts...");
        }

        void renameScript(const std::string& oldName, const std::string& newName)
        {
            if (newName.empty() || newName == oldName) return;

            const std::string oldPath = m_scriptsDir + "/" + oldName + ".cpp";
            const std::string newPath = m_scriptsDir + "/" + newName + ".cpp";

            if (std::filesystem::exists(newPath))
            {
                setStatus("ERROR: " + newName + " already exists.");
                return;
            }

            std::error_code ec;
            std::filesystem::rename(oldPath, newPath, ec);
            if (ec)
            {
                setStatus("ERROR: rename failed: " + ec.message());
                return;
            }

            // Patch struct name + DECLARE_SCRIPT inside the file
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

            // Update all live entries
            for (auto& [eid, entry] : m_entityScripts)
                if (entry.scriptName == oldName)
                {
                    entry.scriptName      = newName;
                    entry.comp.scriptName = newName;
                }

            if (m_selectedScript == oldName)
                m_selectedScript = newName;

            m_manager->pollForChanges();
            setStatus("Renamed: " + oldName + " -> " + newName);
        }

        void openScriptsDir()
        {
            // Resolve to absolute so the file manager always opens the right folder.
            std::string absDir = std::filesystem::absolute(m_scriptsDir).string();
            std::filesystem::create_directories(absDir);
            #ifdef _WIN32
                ShellExecuteA(nullptr, "explore", absDir.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            #elif defined(__APPLE__)
                std::system(("open \"" + absDir + "\"").c_str());
            #else
                std::system(("xdg-open \"" + absDir + "\"").c_str());
            #endif
        }

        void setStatus(const std::string& msg)
        {
            m_statusMsg   = msg;
            m_statusTimer = k_statusDuration;
        }

        // ── Data ──────────────────────────────────────────────────────────────────

        ScriptHotReloadManager* m_manager      = nullptr;
        ECS::Registry*          m_registry     = nullptr;
        float                   m_deltaTime    = 0.f;

        std::unordered_map<ECS::EntityID, AttachedScript> m_entityScripts;
        std::unordered_map<ECS::EntityID, std::string>    m_attachCombo;

        // Rename state
        int  m_renamingIndex  = -1;
        char m_renameBuffer[128] = {};

        std::string m_selectedScript;
        std::string m_scriptsDir;
        std::string m_cmakeBuildDir;

        std::string            m_statusMsg;
        float                  m_statusTimer    = 0.f;
        static constexpr float k_statusDuration = 3.f;
    };
} // namespace gcep
