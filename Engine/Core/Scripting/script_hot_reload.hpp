#pragma once

// Internals
#include "script_interface.hpp"
#include <config.hpp>
#include <ECS/Components/script_component.hpp>
#include <ECS/headers/component_registry.hpp>
#include <ECS/headers/registry.hpp>
#include <Log/log.hpp>

// STL
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <string>
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
                                  ECS::Registry* registry) const;

        void hotReload(const std::string& name,
                       std::vector<panel::AttachedScript*>& activeScripts,
                       std::function<ScriptContext(unsigned int entityId)> makeCtx);

    private:

        std::string m_scriptsDir;
        std::string m_buildDir;
        std::string m_includeDir;

        std::vector<LoadedScript>               m_scripts;
        std::unordered_map<std::string, size_t> m_nameToIndex;

        #ifdef _WIN32
            static std::string findVcvarsall();
        #endif

        bool compileScript      (const std::string& sourcePath, const std::string& outputPath);
        void loadOrReloadScripts(const std::string& sourcePath);
    };

} // namespace gcep
