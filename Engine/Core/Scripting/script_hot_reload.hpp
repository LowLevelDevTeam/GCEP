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

/// @brief cross platform library loading
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
    using FnCreate   = ScriptInstance(*)();
    using FnDestroy  = void(*)(ScriptInstance);
    using FnOnStart  = void(*)(ScriptInstance, const ScriptContext*);
    using FnOnUpdate = void(*)(ScriptInstance, const ScriptContext*);
    using FnOnEnd    = void(*)(ScriptInstance, const ScriptContext*);

    /// @brief Loaded script data
    struct LoadedScript
    {
        std::string name;
        std::string sourcePath;
        std::string libPath;
        LibHandle handle     = nullptr;
        FnCreate    create   = nullptr;
        FnDestroy   destroy  = nullptr;
        FnOnStart   onStart  = nullptr;
        FnOnUpdate  onUpdate = nullptr;
        FnOnEnd     onEnd    = nullptr;

        std::filesystem::file_time_type lastWriteTime;
        bool valid = false;

        static inline bool _gcep_registered =
            false;
    };

    class ScriptHotReloadManager
    {
    public:

        void init(const std::string& scriptsDir, const std::string& buildDir, const std::string& includeDir);
        void shutdown();
        void pollForChanges();
        void renameScript(const std::string& oldName, const std::string& newName);

        std::vector<std::string> getAvailableScriptNames() const;
        LoadedScript*            getScript(const std::string& name);

        void createInstance (ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, ECS::Registry* registry = nullptr);
        void destroyInstance(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, ECS::Registry* registry = nullptr);

        void callOnStart (ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx);
        void callOnUpdate(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx);
        void callOnEnd   (ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx);

        /// @brief Builds a ScriptContext with ECS function pointers wired up.
        /// Call this instead of constructing ScriptContext manually.
        ScriptContext makeContext(unsigned int entityId, float deltaTime,
                                  ECS::Registry* registry, gcep::InputSystem* input = nullptr) const;

        void hotReload(const std::string& name,
                       std::vector<panel::AttachedScript*>& activeScripts,
                       std::function<ScriptContext(unsigned int entityId)> makeCtx);

        /// @brief Commits scripts that finished background compilation to the main thread.
        void commitPending();

        /// @brief True while background compilation is in progress.
        bool isCompiling() const { return m_compiling.load(); }

        std::function<void(const std::string& scriptName)> m_onScriptReady;

    private:

        std::string m_scriptsDir;
        std::string m_buildDir;
        std::string m_includeDir;

        std::vector<LoadedScript>               m_scripts;
        std::unordered_map<std::string, size_t> m_nameToIndex;

        #ifdef _WIN32
            static std::string findVcvarsall();
        #endif

private:
    // Synchronous compile only — no lib_load. Safe to call from any thread.
    bool compileScript(const std::string& sourcePath, const std::string& outputPath);

    // Main-thread only: lib_load + register into m_scripts.
    // Called by commitPending().
    void commitScript(const std::string& sourcePath);

    // Schedules a compile on the worker thread then queues for commitScript.
    // Replaces the old loadOrReloadScripts.
    void scheduleReload(const std::string& sourcePath);

    // Unloads the old handle synchronously (main thread).
    // Must be called BEFORE scheduleReload for the same script.
    void unloadScript(const std::string& name);

        std::thread             m_compileThread;
        std::atomic<bool>       m_compiling { false };
        std::mutex              m_scriptsMutex;

        // Scripts that finished compiling on the worker and need to be
        // committed to m_scripts on the main thread next poll.
        struct PendingScript
        {
            std::string sourcePath;
        };
        std::vector<PendingScript>  m_pendingLoads;
        std::mutex                  m_pendingMutex;
    };

} // namespace gcep
