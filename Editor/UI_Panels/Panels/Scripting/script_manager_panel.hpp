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
    /// @brief Docked "Script Manager" editor window.
    ///
    /// Implements @c IPanel and is called unconditionally from @c uiUpdate() every
    /// frame. Provides a global view of all loaded scripts and their hot-reload
    /// state, per-script reload and rename controls, a bulk "Compile All" action,
    /// a shortcut to open the scripts directory in the system file browser, and a
    /// table mapping scene entities to their currently attached scripts.
    ///
    /// Like @c EntityScriptPanel, this panel does not own @c ScriptState — it
    /// receives a reference at construction time.
    class ScriptManagerPanel : public IPanel
    {
    public:
        /// @brief Constructs the panel with a reference to the shared scripting state.
        ///
        /// @param state  Shared @c ScriptState owned by @c UiManager. Must outlive this panel.
        explicit ScriptManagerPanel(ScriptState& state);

        /// @brief Renders the Script Manager ImGui window.
        ///
        /// Draws the script list with per-row reload/rename buttons, the "Compile
        /// All" and "Open Scripts Dir" toolbar actions, the compilation status
        /// overlay, and the entity-to-script mapping table. Called every frame by
        /// @c uiUpdate().
        void draw() override;

        // ── Simulation lifecycle ──────────────────────────────────────────────

        /// @brief Called by @c UiManager when the simulation transitions to the running state.
        ///
        /// Calls @c scriptOnStart() for every entity that has an attached script,
        /// using a @c ScriptContext built from the current @c ScriptState.
        void onSimulationStart();

        /// @brief Called by @c UiManager every simulation tick.
        ///
        /// Calls @c scriptOnUpdate() for all attached scripts and decrements the
        /// status-message timer in @c ScriptState.
        ///
        /// @param dt  Frame delta time in seconds.
        void onSimulationUpdate(float dt);

        /// @brief Called by @c UiManager when the simulation stops.
        ///
        /// Calls @c scriptOnEnd() for all attached scripts and destroys their
        /// live instances, leaving the @c AttachedScript records intact for the
        /// next simulation run.
        void onSimulationStop();

        /// @brief Returns a reference to the shared @c ScriptState.
        ///
        /// Allows @c UiManager (or other owning systems) to access and mutate the
        /// state after the panel has been constructed.
        ///
        /// @return Mutable reference to the panel's @c ScriptState.
        ScriptState& getState() { return m_state; }

    private:
        /// @brief Reloads all known scripts, optionally filtered by name.
        ///
        /// Iterates over every entry in @c ScriptHotReloadManager, unloads the
        /// current library, schedules a recompile, and calls @c hotReload() for
        /// entities whose attached script matches the filter. An empty @p filter
        /// string reloads every script unconditionally.
        ///
        /// @param filter  Substring to match against script names; empty means "all".
        void reloadAllScripts(const std::string& filter = "");

        /// @brief Renames a script on disk and updates all references to it.
        ///
        /// Renames the source file from @p oldName to @p newName, updates the
        /// @c LoadedScript entry in the hot-reload manager, and patches every
        /// @c AttachedScript in @c ScriptState that references @p oldName.
        ///
        /// @param oldName  Current script name (must exist in the manager).
        /// @param newName  Desired new name (must not already exist).
        void renameScript(const std::string& oldName, const std::string& newName);

        /// @brief Triggers a full compile pass of all scripts in @c m_state.scriptsDir.
        ///
        /// Schedules background compilation via @c ScriptHotReloadManager for every
        /// @c .cpp file found in the scripts directory. Results become available the
        /// next time @c commitPending() is called (i.e., on the next @c pollForChanges()).
        void compileScripts();

        /// @brief Opens @c m_state.scriptsDir in the system's default file browser.
        ///
        /// On Windows uses @c ShellExecuteA; on macOS uses @c open; on Linux uses
        /// @c xdg-open. Does nothing if @c scriptsDir is empty.
        void openScriptsDir();

        ScriptState& m_state; ///< Non-owning reference to the shared scripting state.
    };

} // namespace gcep::panel
