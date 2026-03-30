#pragma once

// Internals
#include <config.hpp>
#include <ECS/Components/script_component.hpp>
#include <ECS/headers/component_registry.hpp>
#include <ECS/headers/registry.hpp>
#include <Log/log.hpp>
#include <Scripting/API/script_interface.hpp>

// STL
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Forward declarations
namespace gcep::panel
{
    struct AttachedScript;
}

/// @brief _NSGetExecutablePath (macOS only)
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

/// @brief Cross-platform shared-library loading abstraction.
///
/// Provides @c lib_load / @c lib_unload / @c lib_symbol / @c lib_extension as
/// thin wrappers over @c LoadLibraryA / @c FreeLibrary / @c GetProcAddress on
/// Windows and @c dlopen / @c dlclose / @c dlsym on POSIX. @c lib_extension()
/// returns the platform-appropriate suffix (@c .dll / @c .dylib / @c .so).
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    using LibHandle = HMODULE;

    inline static LibHandle   lib_load(const char* path) { return LoadLibraryA(path); }
    inline static void        lib_unload(LibHandle h)    { if (h) FreeLibrary(h); }
    inline static void*       lib_symbol(LibHandle h, const char* name) { return (void*)GetProcAddress(h, name); }
    inline static std::string lib_extension() { return ".dll"; }
#else
    #include <dlfcn.h>
    using LibHandle = void*;

    inline static LibHandle   lib_load(const char* path) { return dlopen(path, RTLD_NOW); }
    inline static void        lib_unload(LibHandle h)    { if (h) dlclose(h); }
    inline static void*       lib_symbol(LibHandle h, const char* name) { return dlsym(h, name); }
    #ifdef __APPLE__
        inline static std::string lib_extension() { return ".dylib"; }
    #else
        inline static std::string lib_extension() { return ".so"; }
    #endif
#endif

// Forward declarations
namespace gcep
{
    class InputSystem;
}

namespace gcep
{
    /// @brief Function-pointer typedefs for the five symbols exported by every script library.
    /// @{
    using FnCreate   = ScriptInstance(*)();                              ///< @c scriptCreate   — allocates a new script instance.
    using FnDestroy  = void(*)(ScriptInstance);                          ///< @c scriptDestroy  — frees a script instance.
    using FnOnStart  = void(*)(ScriptInstance, const ScriptContext*);    ///< @c scriptOnStart  — simulation-start callback.
    using FnOnUpdate = void(*)(ScriptInstance, const ScriptContext*);    ///< @c scriptOnUpdate — per-frame update callback.
    using FnOnEnd    = void(*)(ScriptInstance, const ScriptContext*);    ///< @c scriptOnEnd    — simulation-stop callback.
    /// @}

    /// @brief Runtime record for one loaded script shared library.
    ///
    /// Populated by @c ScriptHotReloadManager::commitScript() once a background
    /// compile has succeeded and the library has been loaded into the process.
    /// @c valid is set to @c true only when all five function pointers are
    /// resolved successfully.
    struct LoadedScript
    {
        std::string name;       ///< Logical script name (stem of the source file, without extension).
        std::string sourcePath; ///< Absolute path to the @c .cpp source file.
        std::string libPath;    ///< Absolute path to the compiled shared library on disk.
        LibHandle   handle    = nullptr; ///< OS handle returned by @c lib_load(); @c nullptr if not loaded.
        FnCreate    create    = nullptr; ///< Resolved @c scriptCreate symbol.
        FnDestroy   destroy   = nullptr; ///< Resolved @c scriptDestroy symbol.
        FnOnStart   onStart   = nullptr; ///< Resolved @c scriptOnStart symbol.
        FnOnUpdate  onUpdate  = nullptr; ///< Resolved @c scriptOnUpdate symbol.
        FnOnEnd     onEnd     = nullptr; ///< Resolved @c scriptOnEnd symbol.

        /// @brief Timestamp of the source file at the time of the last successful load.
        ///
        /// Used by @c pollForChanges() to detect modifications since the last compile.
        std::filesystem::file_time_type lastWriteTime;

        bool valid = false; ///< @c true when all five symbols resolved and the library is ready for use.

        /// @brief Guard flag preventing duplicate registration of the ECS component.
        static inline bool _gcep_registered = false;
    };

    /// @brief Manages compilation, loading, and hot-reloading of user script shared libraries.
    ///
    /// Monitors a directory of @c .cpp script sources and automatically recompiles
    /// and reloads them when file-system changes are detected. Compilation runs on
    /// a background thread; the resulting libraries are loaded on the main thread
    /// via @c commitPending() to avoid racy @c lib_load() calls.
    ///
    /// ### Typical frame loop
    /// @code
    /// // each frame, on the main thread:
    /// manager.pollForChanges();   // detects modifications, schedules compiles
    /// manager.commitPending();    // lib_load() for any newly compiled scripts
    /// @endcode
    class ScriptHotReloadManager
    {
    public:

        /// @brief Initialises the manager and performs an initial compile of all discovered scripts.
        ///
        /// Scans @p scriptsDir for @c .cpp files, compiles them into @p buildDir using
        /// @p includeDir as the header search path, and loads the resulting libraries.
        /// Must be called once before any other method.
        ///
        /// @param scriptsDir  Directory containing user script @c .cpp source files.
        /// @param buildDir    Output directory for compiled shared libraries.
        /// @param includeDir  Include path passed to the compiler for engine headers.
        void init(const std::string& scriptsDir, const std::string& buildDir, const std::string& includeDir);

        /// @brief Unloads all script libraries and joins the compile thread.
        ///
        /// Safe to call from the main thread during application shutdown. After this
        /// call all @c LoadedScript handles are released and the manager must not be used.
        void shutdown();

        /// @brief Checks for source file modifications and schedules recompilation.
        ///
        /// Walks @c m_scriptsDir and compares each file's write time against the
        /// stored @c lastWriteTime. For any file that has changed, @c unloadScript()
        /// is called synchronously and then @c scheduleReload() queues a background
        /// compile. Also ticks the @c statusTimer in attached @c ScriptState if set.
        ///
        /// Must be called from the main thread once per frame.
        void pollForChanges();

        /// @brief Renames a loaded script and updates its on-disk source file.
        ///
        /// Renames the @c .cpp source file from @p oldName to @p newName and updates
        /// the @c LoadedScript entry. All entity @c AttachedScript records that
        /// reference @p oldName are patched via the caller.
        ///
        /// @param oldName  Current script name (must exist).
        /// @param newName  New script name (must not already exist).
        void renameScript(const std::string& oldName, const std::string& newName);

        /// @brief Returns the names of all currently tracked scripts.
        ///
        /// Includes scripts that are compiling or invalid; filter on
        /// @c LoadedScript::valid if you need only ready scripts.
        ///
        /// @return Snapshot vector of script name strings.
        std::vector<std::string> getAvailableScriptNames() const;

        /// @brief Looks up a @c LoadedScript by name.
        ///
        /// @param name  Logical script name to look up.
        /// @return      Pointer to the @c LoadedScript, or @c nullptr if not found.
        LoadedScript* getScript(const std::string& name);

        // ── Instance lifecycle ────────────────────────────────────────────────

        /// @brief Allocates a script instance and wires it into @p comp and @p runtime.
        ///
        /// Resolves the @c scriptCreate symbol from the library named by
        /// @c comp.scriptName, calls it, and stores the returned opaque pointer in
        /// @p runtime. If @p registry is provided, the script component type is
        /// registered with the ECS if it has not been already.
        ///
        /// @param comp     Script component storing the script name.
        /// @param runtime  Runtime data that receives the new instance pointer.
        /// @param registry Optional ECS registry for component registration.
        void createInstance(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, ECS::Registry* registry = nullptr);

        /// @brief Destroys the live script instance stored in @p runtime.
        ///
        /// Calls the @c scriptDestroy symbol and nulls out the instance pointer in
        /// @p runtime. Safe to call even if no instance is currently alive.
        ///
        /// @param comp     Script component (provides the script name for lookup).
        /// @param runtime  Runtime data whose instance should be destroyed.
        /// @param registry Optional ECS registry (unused currently, reserved for future cleanup).
        void destroyInstance(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, ECS::Registry* registry = nullptr);

        // ── Script callbacks ──────────────────────────────────────────────────

        /// @brief Dispatches @c scriptOnStart() to the script held by @p runtime.
        ///
        /// @param comp     Identifies which @c LoadedScript to look up.
        /// @param runtime  Provides the instance pointer.
        /// @param ctx      Context passed verbatim to the script callback.
        void callOnStart(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx);

        /// @brief Dispatches @c scriptOnUpdate() to the script held by @p runtime.
        ///
        /// Called once per simulation tick. The @c ScriptContext's @c deltaTime
        /// should reflect the current frame's elapsed seconds.
        ///
        /// @param comp     Identifies which @c LoadedScript to look up.
        /// @param runtime  Provides the instance pointer.
        /// @param ctx      Context passed verbatim to the script callback.
        void callOnUpdate(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx);

        /// @brief Dispatches @c scriptOnEnd() to the script held by @p runtime.
        ///
        /// @param comp     Identifies which @c LoadedScript to look up.
        /// @param runtime  Provides the instance pointer.
        /// @param ctx      Context passed verbatim to the script callback.
        void callOnEnd(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx);

        /// @brief Constructs a fully populated @c ScriptContext with all ECS function-pointer tables wired up.
        ///
        /// Prefer this factory over constructing @c ScriptContext manually. Sets
        /// @c entityId, @c deltaTime, @c registry, @c ecs, @c cameraApi, @c inputApi,
        /// and @c input from the supplied arguments and the engine's singleton API tables.
        ///
        /// @param entityId  ID of the entity the context is built for.
        /// @param deltaTime Frame delta time in seconds.
        /// @param registry  ECS registry the script will operate on.
        /// @param input     Optional input system pointer; may be @c nullptr if input is unavailable.
        /// @return          A fully populated @c ScriptContext ready for dispatch.
        ScriptContext makeContext(unsigned int entityId, float deltaTime,
                                  ECS::Registry* registry, gcep::InputSystem* input = nullptr) const;

        /// @brief Hot-reloads a named script and restarts any entities that were running it.
        ///
        /// Unloads the old library, waits for any pending compile to complete,
        /// loads the new library, then iterates over @p activeScripts. For each
        /// entry whose @c scriptName matches @p name, destroys the old instance,
        /// calls @c createInstance(), and calls @c callOnStart() with a context
        /// produced by @p makeCtx.
        ///
        /// @param name          Script to reload (must match a known @c LoadedScript name).
        /// @param activeScripts List of currently attached scripts across all entities.
        /// @param makeCtx       Callable that produces a @c ScriptContext given an entity ID.
        void hotReload(const std::string& name,
                       std::vector<panel::AttachedScript*>& activeScripts,
                       std::function<ScriptContext(unsigned int entityId)> makeCtx);

        /// @brief Loads any scripts that finished background compilation into @c m_scripts.
        ///
        /// Drains @c m_pendingLoads under @c m_pendingMutex and calls
        /// @c commitScript() for each entry. Fires @c m_onScriptReady if set.
        /// Must be called from the main thread (calls @c lib_load() internally).
        void commitPending();

        /// @brief Returns @c true while a background compilation is in progress.
        bool isCompiling() const { return m_compiling.load(); }

        /// @brief Optional callback invoked on the main thread when a script becomes ready.
        ///
        /// Set by the owning panel system to trigger UI refresh. The argument is
        /// the logical script name that just finished loading.
        std::function<void(const std::string& scriptName)> m_onScriptReady;

    private:

        std::string m_scriptsDir;  ///< Root directory scanned for @c .cpp sources.
        std::string m_buildDir;    ///< Output directory for compiled shared libraries.
        std::string m_includeDir;  ///< Header search path forwarded to the compiler.

        std::vector<LoadedScript>               m_scripts;     ///< All tracked scripts (valid and invalid).
        std::unordered_map<std::string, size_t> m_nameToIndex; ///< Fast name-to-index lookup into @c m_scripts.

    private:
        /// @brief Synchronously compiles @p sourcePath to @p outputPath.
        ///
        /// Invokes the system compiler via @c std::system(). Returns @c true on
        /// success (exit code 0), @c false otherwise. Safe to call from any thread
        /// as it does not touch @c m_scripts.
        ///
        /// @param sourcePath  Absolute path to the @c .cpp source file.
        /// @param outputPath  Destination path for the resulting shared library.
        /// @return            @c true if the compile succeeded.
        bool compileScript(const std::string& sourcePath, const std::string& outputPath);

        /// @brief Main-thread only: @c lib_load() and register into @c m_scripts.
        ///
        /// Resolves all five function-pointer symbols from the compiled library at
        /// @p sourcePath and either updates an existing @c LoadedScript entry or
        /// appends a new one. Sets @c LoadedScript::valid accordingly.
        /// Called exclusively by @c commitPending().
        ///
        /// @param sourcePath  Absolute path to the @c .cpp source; the library path is derived from this.
        void commitScript(const std::string& sourcePath);

        /// @brief Queues a background compile of @p sourcePath via @c m_compileThread.
        ///
        /// If a compile is already running the new job is enqueued; otherwise the
        /// worker thread is launched immediately. On completion the source path is
        /// pushed onto @c m_pendingLoads for @c commitPending() to pick up.
        ///
        /// @param sourcePath  Absolute path to the @c .cpp source file to compile.
        void scheduleReload(const std::string& sourcePath);

        /// @brief Unloads the shared library for the named script synchronously.
        ///
        /// Calls @c lib_unload(), nulls all function pointers, and marks the
        /// @c LoadedScript as invalid. Must be called from the main thread before
        /// @c scheduleReload() to avoid accessing a stale handle from the old library.
        ///
        /// @param name  Logical name of the script to unload.
        void unloadScript(const std::string& name);

        std::thread       m_compileThread;      ///< Worker thread that runs @c compileScript().
        std::atomic<bool> m_compiling { false }; ///< @c true while the worker thread is active.
        std::mutex        m_scriptsMutex;        ///< Protects @c m_scripts and @c m_nameToIndex.

        /// @brief Script that finished background compilation and awaits @c commitScript().
        struct PendingScript
        {
            std::string sourcePath; ///< Absolute path to the compiled @c .cpp source.
        };

        std::vector<PendingScript> m_pendingLoads;  ///< Queue of scripts ready to be committed on the main thread.
        std::mutex                 m_pendingMutex;  ///< Protects @c m_pendingLoads for cross-thread access.
    };

} // namespace gcep
