#pragma once

// Internals
#include <Scripting/script_hot_reload.hpp>
#include <ECS/headers/registry.hpp>

// STL
#include <string>
#include <unordered_map>

namespace gcep::panel
{
    struct AttachedScript
    {
        std::string     scriptName;
        ScriptComponent comp;
    };

    /// @brief Shared state between EntityScriptPanel and ScriptManagerPanel.
    /// Owned by whoever initialises the scripting system (e.g. UiManager).
    struct ScriptState
    {
        ScriptHotReloadManager* manager       = nullptr;
        ECS::Registry*          registry      = nullptr;
        float                   deltaTime     = 0.f;
        std::string             scriptsDir;
        std::string             cmakeBuildDir;

        std::unordered_map<ECS::EntityID, AttachedScript> entityScripts;
        std::unordered_map<ECS::EntityID, std::string>    attachCombo;

        std::string            statusMsg;
        float                  statusTimer    = 0.f;
        static constexpr float k_statusDuration = 3.f;

        // Rename state (used by ScriptManagerPanel)
        int  renamingIndex     = -1;
        char renameBuffer[128] = {};
        std::string selectedScript;

        void setStatus(const std::string& msg)
        {
            statusMsg   = msg;
            statusTimer = k_statusDuration;
        }

        ScriptContext makeCtx(ECS::EntityID eid) const
        {
            return manager->makeContext(
                static_cast<unsigned int>(eid), deltaTime, registry);
        }
    };

} // namespace gcep::panel
