#include "script_hot_reload.hpp"

// Internals
#include <Scripting/Bridges/script_ecs_bridge.hpp>
#include <Scripting/Bridges/script_input_bridge.hpp>
#include <Editor/Inputs/inputs.hpp>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>

namespace gcep
{
    void ScriptHotReloadManager::init(const std::string& scriptsDir, const std::string& buildDir, const std::string& includeDir)
    {
        m_scriptsDir = scriptsDir;
        m_buildDir   = buildDir;
        m_includeDir = includeDir;

        std::filesystem::create_directories(m_buildDir);

        std::vector<std::string> sources;
        for (auto& entry : std::filesystem::directory_iterator(m_scriptsDir))
            if (entry.path().extension() == ".cpp")
                sources.push_back(entry.path().string());

        if (sources.empty()) return;

        for (auto& src : sources)
        {
            std::string name = std::filesystem::path(src).stem().string();
            if (m_nameToIndex.count(name)) continue;

            LoadedScript stub;
            stub.name          = name;
            stub.sourcePath    = src;
            stub.lastWriteTime = std::filesystem::last_write_time(src);
            stub.valid         = false;
            m_nameToIndex[name] = m_scripts.size();
            m_scripts.push_back(std::move(stub));
        }

        m_compiling = true;
        m_compileThread = std::thread([this, sources = std::move(sources)]()
        {
            for (auto& src : sources)
            {
                std::string name    = std::filesystem::path(src).stem().string();
                std::string libPath = m_buildDir + "/" + name + lib_extension();

                if (!compileScript(src, libPath))
                {
                    Log::error(std::format("Failed to compile script {} on init", src));
                    continue;
                }
                {
                    std::lock_guard lock(m_pendingMutex);
                    m_pendingLoads.push_back({ src });
                }
            }
            m_compiling = false;
            Log::info("Background script compilation finished.");
        });

        m_compileThread.detach();
    }

    void ScriptHotReloadManager::shutdown()
    {
        // Wait for any background compile to finish before unloading
        if (m_compileThread.joinable())
            m_compileThread.join();

        for (auto& script : m_scripts)
        {
            lib_unload(script.handle);
            script.handle = nullptr;
            script.valid  = false;
        }

        m_scripts.clear();
        m_nameToIndex.clear();
    }

    void ScriptHotReloadManager::pollForChanges()
    {
        commitPending();
        if (m_compiling) return;

        for (auto& entry : std::filesystem::directory_iterator(m_scriptsDir))
        {
            if (entry.path().extension() != ".cpp") continue;

            std::string path = entry.path().string();
            std::string name = entry.path().stem().string();

            auto it = m_nameToIndex.find(name);
            if (it == m_nameToIndex.end())
                scheduleReload(path);
            else
            {
                auto currentTime = std::filesystem::last_write_time(entry.path());
                if (currentTime != m_scripts[it->second].lastWriteTime)
                {
                    Log::info(std::format("Modification detected : {}, reloading script.", path));
                    scheduleReload(path);
                }
            }
        }
    }

    void ScriptHotReloadManager::renameScript(const std::string& oldName, const std::string& newName)
    {
        auto it = m_nameToIndex.find(oldName);
        if (it == m_nameToIndex.end()) return;

        std::size_t idx = it->second;
        LoadedScript& script = m_scripts[idx];

        // Unload the old library
        if (script.handle)
        {
            lib_unload(script.handle);
            script.handle = nullptr;
        }

        // Update the script's own name/path fields
        script.name = newName;
        script.sourcePath = m_scriptsDir + "/" + newName + ".cpp";
        script.libPath = m_buildDir + "/" + newName + lib_extension();
        script.valid = false;

        // Re-key the name map
        m_nameToIndex.erase(it);
        m_nameToIndex[newName] = idx;
    }

    std::vector<std::string> ScriptHotReloadManager::getAvailableScriptNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_scripts.size());
        for (auto& s : m_scripts)
        {
            if (s.valid) names.push_back(s.name);
        }
        return names;
    }

    LoadedScript* ScriptHotReloadManager::getScript(const std::string& name)
    {
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end()) return nullptr;
        auto& s = m_scripts[it->second];
        return s.valid ? &s : nullptr;
    }

    void ScriptHotReloadManager::createInstance(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, ECS::Registry* registry)
    {
        auto* script = getScript(comp.scriptName);
        if (!script || !script->create) return;

        runtime.instance = script->create();
        runtime.loadedScriptIndex = static_cast<int>(m_nameToIndex[comp.scriptName]);
        runtime.started = false;

        if(!registry) return;

        // Sync back into the ECS component if a registry is provided
        if (!registry->hasComponent<ECS::ScriptComponent>(comp.entityId))
        {
            auto& c = registry->addComponent<ECS::ScriptComponent>(comp.entityId);
            c = comp;
        }
        else
        {
            auto& c = registry->getComponent<ECS::ScriptComponent>(comp.entityId);
            c = comp;
        }
    }

    void ScriptHotReloadManager::destroyInstance(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, ECS::Registry* registry)
    {
        auto* script = getScript(comp.scriptName);
        if (script && script->destroy && runtime.instance)
            script->destroy(runtime.instance);

        runtime.instance          = nullptr;
        runtime.started           = false;
        runtime.loadedScriptIndex = -1;

        if(!registry) return;

        // Sync back into the ECS component if a registry is provided
        if (registry->hasComponent<ECS::ScriptComponent>(comp.entityId))
        {
            registry->removeComponent<ECS::ScriptComponent>(comp.entityId);
        }
    }

    void ScriptHotReloadManager::callOnStart(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx)
    {
        if (runtime.started) return;
        auto* script = getScript(comp.scriptName);
        if (script && script->onStart && runtime.instance)
        {
            script->onStart(runtime.instance, &ctx);
            runtime.started = true;
        }
    }

    void ScriptHotReloadManager::callOnUpdate(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx)
    {
        auto* script = getScript(comp.scriptName);
        if (script && script->onUpdate && runtime.instance)
        {
            script->onUpdate(runtime.instance, &ctx);
        }
    }

    void ScriptHotReloadManager::callOnEnd(ECS::ScriptComponent& comp, ECS::ScriptRuntimeData& runtime, const ScriptContext& ctx)
    {
        auto* script = getScript(comp.scriptName);
        if (script && script->onEnd && runtime.instance)
        {
            script->onEnd(runtime.instance, &ctx);
        }
    }

    ScriptContext ScriptHotReloadManager::makeContext(ECS::EntityID entityId, float deltaTime, ECS::Registry* registry, gcep::InputSystem* input) const
    {
        ScriptContext ctx{};
        ctx.entityId  = entityId;
        ctx.deltaTime = deltaTime;
        ctx.registry  = registry;
        ctx.ecs       = getScriptECSAPI();
        ctx.input      = input;
        ctx.inputApi   = input ? getScriptInputAPI() : nullptr;
        ctx.cameraApi  = getScriptCameraAPI();

        return ctx;
    }

    void ScriptHotReloadManager::hotReload(const std::string& name,
                                           std::vector<panel::AttachedScript*>& activeScripts,
                                           std::function<ScriptContext(ECS::EntityID entityId)> makeCtx)
    {
        auto* script = getScript(name);
        if (!script) return;

        for (auto* attached : activeScripts)
        {
            if (attached->comp.scriptName != name || !attached->runtime.instance) continue;
            ScriptContext ctx = makeCtx(attached->comp.entityId);
            callOnEnd(attached->comp, attached->runtime, ctx);
            destroyInstance(attached->comp, attached->runtime);
        }

        scheduleReload(script->sourcePath);

        for (auto* attached : activeScripts)
        {
            if (attached->comp.scriptName != name) continue;
            attached->runtime.started = false;
            attached->runtime.instance = nullptr;
        }
    }

    static std::string resolveCompiler()
    {
        namespace fs = std::filesystem;

    #ifdef _WIN32
        char exeBuf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        fs::path exeDir = fs::path(exeBuf).parent_path();
        fs::path bundled = exeDir / "bin" / "clang++.exe";
    #elif defined(__APPLE__)
        char exeBuf[PATH_MAX] = {};
        uint32_t sz = PATH_MAX;
        _NSGetExecutablePath(exeBuf, &sz);
        fs::path exeDir = fs::path(exeBuf).parent_path();
        fs::path bundled = exeDir / "bin" / "clang++";
    #else
        fs::path exeDir = fs::read_symlink("/proc/self/exe").parent_path();
        fs::path bundled = exeDir / "bin" / "clang++";
    #endif

        if (fs::exists(bundled))
            return bundled.string();

        Log::error(std::format("Bundled compiler not found at '{}'. "
                               "Place clang++ in <exe>/bin/ next to the engine binary.",
                               bundled.string()));
        return {};
    }

    static std::string resolveBase()
    {
        namespace fs = std::filesystem;

    #ifdef _WIN32
        char exeBuf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        fs::path exeDir = fs::path(exeBuf).parent_path();
    #elif defined(__APPLE__)
        char exeBuf[PATH_MAX] = {};
        uint32_t sz = PATH_MAX;
        _NSGetExecutablePath(exeBuf, &sz);
        fs::path exeDir = fs::path(exeBuf).parent_path();
    #else
        fs::path exeDir = fs::read_symlink("/proc/self/exe").parent_path();
    #endif
    
        return (exeDir / "bin").lexically_normal().string();
    }

    bool ScriptHotReloadManager::compileScript(const std::string& sourcePath, const std::string& outputPath)
    {
        namespace fs = std::filesystem;

        const std::string cxx = resolveCompiler();
        if (cxx.empty())
        {
            Log::error("No bundled clang++ found. Place clang++ in <exe>/bin/.");
            return false;
        }

        const std::string base        = resolveBase();
        const std::string resourceDir = base + "/lib/clang/22";

        std::string cmd;

        #ifdef _WIN32
        {
            cmd = std::string("cmd /C ")
                + "\""
                    + "\"" + cxx + "\""
                    + " -std=c++20 -O2 -shared -fvisibility=hidden"
                    + " -resource-dir \"" + resourceDir  + "\""
                    + " -I\""             + m_includeDir + "\""
                    + " \""               + sourcePath   + "\""
                    + " -o \""            + outputPath   + "\""
                + "\"";
        }
        #else
        {
            cmd = "\"" + cxx + "\" -std=c++20 -O2 -shared -fPIC -fvisibility=hidden"
                + " -resource-dir \"" + resourceDir  + "\""
                + " -I\""             + m_includeDir + "\""
                + " \""               + sourcePath   + "\""
                + " -o \""            + outputPath   + "\"";
        }
        #endif

        Log::info(std::format("Compiling: {}", cmd));
        int result = std::system(cmd.c_str());

        if (result != 0)
        {
            Log::error(std::format("Cannot compile {}", sourcePath));
            return false;
        }

        // Clean up build artifacts.
        const std::filesystem::path outDir(m_buildDir);
        const std::string           stem = std::filesystem::path(sourcePath).stem().string();
        for (const char* ext : { ".obj", ".lib", ".exp" })
        {
            std::filesystem::path artifact = outDir / (stem + ext);
            if (std::filesystem::exists(artifact))
                std::filesystem::remove(artifact);
        }

        return true;
    }

    void ScriptHotReloadManager::scheduleReload(const std::string& sourcePath)
    {
        std::string name    = std::filesystem::path(sourcePath).stem().string();
        std::string libPath = m_buildDir + "/" + name + lib_extension();

        // Update write-time stamp and mark invalid immediately so the UI
        // shows "compiling" rather than stale state
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end())
        {
            auto& s        = m_scripts[it->second];
            s.lastWriteTime = std::filesystem::last_write_time(sourcePath);
            s.valid         = false;
            lib_unload(s.handle);
            s.handle        = nullptr;
        }
        else
        {
            // Register a stub so getAvailableScriptNames() shows it immediately
            LoadedScript stub;
            stub.name          = name;
            stub.sourcePath    = sourcePath;
            stub.lastWriteTime = std::filesystem::last_write_time(sourcePath);
            stub.valid         = false;
            m_nameToIndex[name] = m_scripts.size();
            m_scripts.push_back(std::move(stub));
        }

        // Fire compile on a detached worker; result goes into m_pendingLoads
        m_compiling = true;
        std::thread([this, sourcePath, libPath]()
        {
            if (compileScript(sourcePath, libPath))
            {
                std::lock_guard lock(m_pendingMutex);
                m_pendingLoads.push_back({ sourcePath });
            }
            else
            {
                Log::error(std::format("scheduleReload: compile failed for {}", sourcePath));
            }

            // Only clear the flag when ALL pending compiles are done.
            // Simple approach: recount — if nothing left pending we're idle.
            // (For a single-script reload this fires immediately.)
            m_compiling = false;
        }).detach();
    }

    void ScriptHotReloadManager::commitPending()
    {
        std::vector<PendingScript> pending;
        {
            std::lock_guard lock(m_pendingMutex);
            pending.swap(m_pendingLoads);
        }

        for (auto& p : pending)
        {
            std::string name    = std::filesystem::path(p.sourcePath).stem().string();
            std::string libPath = m_buildDir + "/" + name + lib_extension();

            LibHandle handle = lib_load(libPath.c_str());
            if (!handle)
            {
                Log::error(std::format("commitPending: failed to load {}", libPath));
                continue;
            }

            LoadedScript script;
            script.name          = name;
            script.sourcePath    = p.sourcePath;
            script.libPath       = libPath;
            script.handle        = handle;
            script.create        = (FnCreate)   lib_symbol(handle, "scriptCreate");
            script.destroy       = (FnDestroy)  lib_symbol(handle, "scriptDestroy");
            script.onStart       = (FnOnStart)  lib_symbol(handle, "scriptOnStart");
            script.onUpdate      = (FnOnUpdate) lib_symbol(handle, "scriptOnUpdate");
            script.onEnd         = (FnOnEnd)    lib_symbol(handle, "scriptOnEnd");
            script.lastWriteTime = std::filesystem::last_write_time(p.sourcePath);
            script.valid         = script.create && script.destroy
                                && script.onStart && script.onUpdate && script.onEnd;

            if (!script.valid)
                Log::error(std::format("commitPending: missing exports in {}", p.sourcePath));
            else
                Log::info(std::format("Script '{}' ready.", name));

            auto it = m_nameToIndex.find(name);
            if (it != m_nameToIndex.end())
                m_scripts[it->second] = std::move(script);
            else
            {
                m_nameToIndex[name] = m_scripts.size();
                m_scripts.push_back(std::move(script));
            }

            // Notify pending callbacks (e.g. hotReload restart)
            if (m_onScriptReady)
                m_onScriptReady(name);
        }
    }

} // namespace gcep
