#pragma once

// Internals
#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>

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

namespace gcep::panel
{
    /// @brief Docked "Script Manager" window — IPanel, called unconditionally from uiUpdate().
    /// Shows all loaded scripts, their state, per-script reload, rename, and the scene entities table.
    class ScriptManagerPanel : public IPanel
    {
    public:
        explicit ScriptManagerPanel(ScriptState& state);

        void draw() override;

        // Simulation lifecycle — forward-called by UiManager
        void onSimulationStart();
        void onSimulationUpdate(float dt);
        void onSimulationStop();

        ScriptState& getState() { return m_state; }

    private:
        void reloadAllScripts(const std::string& filter = "");
        void renameScript(const std::string& oldName, const std::string& newName);
        void compileScripts();
        void openScriptsDir();

        ScriptState& m_state;
    };

} // namespace gcep::panel
