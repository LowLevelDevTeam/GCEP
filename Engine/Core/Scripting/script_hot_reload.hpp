#pragma once

// Internals
#include "script_interface.hpp"
#include <ECS/headers/component_registry.hpp>

// STL
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/// @brief cross platform library loading
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    using LibHandle = HMODULE;

    inline LibHandle   lib_load(const char* path) { return LoadLibraryA(path); }
    inline void        lib_unload(LibHandle h)    { if (h) FreeLibrary(h); }
    inline void*       lib_symbol(LibHandle h, const char* name) { return (void*)GetProcAddress(h, name); }
    inline std::string lib_extension() { return ".dll"; }
#else
    #include <dlfcn.h>
    using LibHandle = void*;

    inline LibHandle   lib_load(const char* path) { return dlopen(path, RTLD_NOW); }
    inline void        lib_unload(LibHandle h)    { if (h) dlclose(h); }
    inline void*       lib_symbol(LibHandle h, const char* name) { return dlsym(h, name); }
    #ifdef __APPLE__
        inline std::string lib_extension() { return ".dylib"; }
    #else
        inline std::string lib_extension() { return ".so"; }
    #endif
#endif

using FnCreate   = ScriptInstance(*)();
using FnDestroy  = void(*)(ScriptInstance);
using FnOnStart  = void(*)(ScriptInstance, const ScriptContext*);
using FnOnUpdate = void(*)(ScriptInstance,const ScriptContext*);
using FnOnEnd    = void(*)(ScriptInstance,const ScriptContext*);

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

/// @brief ECS script component
struct ScriptComponent
{
    std::string scriptName;
    ScriptInstance instance = nullptr;
    bool started            = false;
    int loadedScriptIndex   = -1;
    static inline bool _gcep_registered =
        gcep::ECS::ComponentRegistry::instance().reg<ScriptComponent>();
};

class ScriptHotReloadManager
{
    public:

    void init (const std::string& scriptsDir, const std::string& buildDir, const std::string& includeDir)
    {
        m_scriptsDir = scriptsDir;
        m_buildDir   = buildDir;
        m_includeDir = includeDir;

        std::filesystem::create_directories(m_buildDir);

        for (auto& entry : std::filesystem::directory_iterator(m_scriptsDir))
        {
            if (entry.path().extension() == ".cpp")
            {
                loadOrReloadScripts(entry.path().string());
            }
        }
    }

    void shutdown()
    {
        for (auto& script : m_scripts)
        {
            lib_unload(script.handle);
            script.handle = nullptr;
            script.valid = false;
        }

        m_scripts.clear();
        m_nameToIndex.clear();
    }

    void pollForChanges()
    {
        for (auto& entry : std::filesystem::directory_iterator(m_scriptsDir))
        {
            if (entry.path().extension() != ".cpp") continue;

            std::string path = entry.path().string();
            std::string name = entry.path().stem().string();

            auto it = m_nameToIndex.find(name);
            if (it == m_nameToIndex.end())
                loadOrReloadScripts(path);
            else
            {
                auto currentTime = std::filesystem::last_write_time(entry.path());\
                if (currentTime != m_scripts[it->second].lastWriteTime)
                {
                    std::printf("[HotReload] modification detected : %s\n", path.c_str());
                    loadOrReloadScripts(path);
                }
            }
        }
    }

    std::vector<std::string> getAvailableScriptNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_scripts.size());
        for (auto& s : m_scripts)
        {
            if (s.valid) names.push_back(s.name);
        }
        return names;
    }

    LoadedScript* getScript (const std::string& name)
    {
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end()) return nullptr;
        auto& s = m_scripts[it->second];
        return s.valid ? &s : nullptr;
    }

    void createInstance(ScriptComponent& comp)
    {
        auto* script = getScript(comp.scriptName);
        if (!script || !script->create) return;

        comp.instance = script->create();
        comp.loadedScriptIndex = static_cast<int>(m_nameToIndex[comp.scriptName]);
        comp.started = false;
    }

    void destroyInstance(ScriptComponent& comp)
    {
        auto* script = getScript(comp.scriptName);
        if (script && script->destroy && comp.instance)
        {
            script->destroy(comp.instance);
        }
        comp.instance = nullptr;
        comp.started = false;
        comp.loadedScriptIndex = -1;
    }

    void callOnStart(ScriptComponent& comp, const ScriptContext& ctx)
    {
        if (comp.started) return;
        auto* script = getScript(comp.scriptName);
        if (script && script->onStart && comp.instance)
        {
            script->onStart(comp.instance, &ctx);
            comp.started = true;
        }
    }

    void callOnUpdate(ScriptComponent& comp, const ScriptContext& ctx)
    {
        auto* script = getScript(comp.scriptName);
        if (script && script->onUpdate && comp.instance)
        {
            script->onUpdate(comp.instance, &ctx);
        }
    }

    void callOnEnd(ScriptComponent& comp, const ScriptContext& ctx)
    {
        auto* script = getScript(comp.scriptName);
        if (script && script->onEnd && comp.instance)
        {
            script->onEnd(comp.instance, &ctx);
        }
    }


    void hotReload(const std::string& name,
                   std::vector<ScriptComponent*>& activeComponents,
                   std::function<ScriptContext(unsigned int entityId)> makeCtx)
    {
        auto* script = getScript(name);
        if (!script) return;

        for (auto* comp : activeComponents) {
            if (comp->scriptName != name || !comp->instance) continue;
            ScriptContext ctx = makeCtx(comp->loadedScriptIndex);
            callOnEnd(*comp, ctx);
            destroyInstance(*comp);
        }

        loadOrReloadScripts(script->sourcePath);

        for (auto* comp : activeComponents) {
            if (comp->scriptName != name) continue;
            createInstance(*comp);
            ScriptContext ctx = makeCtx(0);
            callOnStart(*comp, ctx);
        }
    }

    private:

    std::string m_scriptsDir;
    std::string m_buildDir;
    std::string m_includeDir;

    std::vector<LoadedScript> m_scripts;
    std::unordered_map<std::string, size_t> m_nameToIndex;

    bool compileScript(const std::string& sourcePath, const std::string& outputPath)
    {
        std::string cmd;

        #ifdef _WIN32
                cmd = "cl /nologo /O2 /LD /EHsc /I\"" + m_includeDir + "\" "
                    + "\"" + sourcePath + "\" /Fe\"" + outputPath + "\" /link /DLL";
        #elif defined(__APPLE__)
                cmd = "clang++ -std=c++17 -O2 -shared -fPIC -fvisibility=hidden "
                      "-I\"" + m_includeDir + "\" "
                      "\"" + sourcePath + "\" -o \"" + outputPath + "\"";
        #else
                cmd = "g++ -std=c++17 -O2 -shared -fPIC -fvisibility=hidden "
                      "-I\"" + m_includeDir + "\" "
                      "\"" + sourcePath + "\" -o \"" + outputPath + "\"";
        #endif

        std::printf("[HotReload] Compilation: %s\n", cmd.c_str());
        int result = system(cmd.c_str());

        if (result != 0)
        {
            std::printf("[HotReload] ERROR: cannot compile %s\n",sourcePath.c_str());
            return false;
        }

        return true;
    }

    void loadOrReloadScripts(const std::string& sourcePath)
    {
        std::filesystem::path src(sourcePath);
        std::string name = src.stem().string();
        std::string libPath = m_buildDir + "/" + name + lib_extension();

        #ifdef _WIN32
                static int reloadCounter = 0;
                std::string actualLibPath = m_buildDir + "/" + name + "_" + std::to_string(reloadCounter++) + lib_extension();
        #else
                std::string actualLibPath = libPath;
        #endif

        if (!compileScript(sourcePath, actualLibPath)) return;

        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end())
        {
            auto& old = m_scripts[it->second];
            lib_unload(old.handle);
            old.handle = nullptr;
            old.valid  = false;
        }

        LibHandle handle = lib_load(actualLibPath.c_str());
        if (!handle)
        {
            std::printf("[HotReload] ERROR: failed to load %s\n",actualLibPath.c_str());
        }

        LoadedScript script;
        script.name = name;
        script.sourcePath = sourcePath;
        script.libPath = actualLibPath;
        script.handle = handle;
        script.create = (FnCreate) lib_symbol(handle, "script_create");
        script.destroy = (FnDestroy) lib_symbol(handle, "script_destroy");
        script.onStart = (FnOnStart) lib_symbol(handle, "script_on_start");
        script.onUpdate = (FnOnUpdate) lib_symbol(handle, "script_on_update");
        script.onEnd = (FnOnEnd) lib_symbol(handle, "script_on_end");
        script.lastWriteTime = std::filesystem::last_write_time(src);
        script.valid = (script.create && script.destroy && script.onStart && script.onUpdate && script.onEnd);

        if (!script.valid)
            std::fprintf(stderr, "[HotReload] ERROR: failed to load (didn't export all functions) %s\n",sourcePath.c_str());
        if (it != m_nameToIndex.end())
            m_scripts[it->second] = std::move(script);
        else
        {
            m_nameToIndex[name] = m_scripts.size();
            m_scripts.push_back(std::move(script));
        }

        std::printf("[HotReload] Script '%s' loaded succesfully \n", name.c_str());
    }
};
