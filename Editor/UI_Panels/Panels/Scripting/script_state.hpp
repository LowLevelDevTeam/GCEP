#pragma once

// Internals
#include <ECS/headers/registry.hpp>
#include <Editor/Inputs/inputs.hpp>
#include <Scripting/script_hot_reload.hpp>

// STL
#include <string>
#include <unordered_map>

namespace gcep::panel
{
    /// @brief Bookkeeping entry for one script attached to a scene entity.
    ///
    /// Pairs the serialisable @c ECS::ScriptComponent (stores the chosen script
    /// name and serialised field values) with the @c ECS::ScriptRuntimeData that
    /// holds the live heap instance returned by @c scriptCreate().
    struct AttachedScript
    {
        std::string            scriptName; ///< Name key that indexes into @c ScriptHotReloadManager.
        ECS::ScriptComponent   comp;       ///< Serialisable component (script name, field overrides).
        ECS::ScriptRuntimeData runtime;    ///< Live instance handle and associated runtime state.
    };

    /// @brief Shared scripting state consumed by both @c EntityScriptPanel and @c ScriptManagerPanel.
    ///
    /// Owns the authoritative mapping from entity IDs to their attached scripts and
    /// per-entity combo-box selection strings. Neither panel owns this struct —
    /// it is typically allocated and initialised by @c UiManager and passed by
    /// reference to both panels at construction time.
    ///
    /// @note @c manager and @c registry must be set before calling any method that
    ///       forwards to the hot-reload system.
    struct ScriptState
    {
        ScriptHotReloadManager* manager       = nullptr; ///< Non-owning pointer to the hot-reload manager. Must outlive this struct.
        ECS::Registry*          registry      = nullptr; ///< Non-owning pointer to the ECS registry. Must outlive this struct.
        InputSystem             inputSystem;             ///< Input system instance used when constructing @c ScriptContext objects.
        float                   deltaTime     = 0.f;     ///< Frame delta time (seconds); updated each tick by the owning system.
        std::string             scriptsDir;              ///< Filesystem path to the user scripts source directory.
        std::string             cmakeBuildDir;           ///< Filesystem path to the CMake build directory used for hot-reload compilation.

        /// @brief One entry per entity that has an attached script.
        std::unordered_map<ECS::EntityID, AttachedScript> entityScripts;

        /// @brief Per-entity string currently selected in the "attach script" combo box.
        std::unordered_map<ECS::EntityID, std::string>    attachCombo;

        std::string            statusMsg;                          ///< Text displayed in the status bar overlay.
        float                  statusTimer    = 0.f;              ///< Remaining display time for @c statusMsg (seconds).
        static constexpr float k_statusDuration = 3.f;           ///< How long (seconds) a status message is shown before fading.

        // ── Rename state (used by ScriptManagerPanel) ─────────────────────────

        int  renamingIndex     = -1;     ///< Index into the script list currently being renamed, or @c -1 if none.
        char renameBuffer[128] = {};     ///< Null-terminated edit buffer for the in-progress rename operation.
        std::string selectedScript;      ///< Name of the script selected in the manager panel's list view.

        /// @brief Posts a timed status message to the overlay.
        ///
        /// Sets @c statusMsg to @p msg and resets @c statusTimer to
        /// @c k_statusDuration so the message is visible for three seconds.
        ///
        /// @param msg  Human-readable message string to display.
        void setStatus(const std::string& msg)
        {
            statusMsg   = msg;
            statusTimer = k_statusDuration;
        }

        /// @brief Constructs a fully populated @c ScriptContext for the given entity.
        ///
        /// Delegates to @c ScriptHotReloadManager::makeContext() using the stored
        /// @c deltaTime, @c registry, and @c inputSystem. Prefer this helper over
        /// constructing a @c ScriptContext manually to ensure all function-pointer
        /// tables are wired correctly.
        ///
        /// @param eid  Entity for which the context is built.
        /// @return     A @c ScriptContext ready to pass to @c callOnStart / @c callOnUpdate / @c callOnEnd.
        ScriptContext makeCtx(ECS::EntityID eid)
        {
            return manager->makeContext(static_cast<unsigned int>(eid), deltaTime, registry, &inputSystem);
        }
    };

} // namespace gcep::panel
