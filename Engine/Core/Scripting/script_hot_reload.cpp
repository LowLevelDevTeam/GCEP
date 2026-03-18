#include "script_hot_reload.hpp"

// Externals
#include <thread>
#include <Editor/UI_Panels/Panels/Scripting/script_state.hpp>

namespace gcep
{
    void ScriptHotReloadManager::init(const std::string& scriptsDir, const std::string& buildDir, const std::string& includeDir)
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

    void ScriptHotReloadManager::shutdown()
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

    void ScriptHotReloadManager::pollForChanges()
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
                auto currentTime = std::filesystem::last_write_time(entry.path());
                if (currentTime != m_scripts[it->second].lastWriteTime)
                {
                    Log::info(std::format("Modification detected : {}, reloading script.", path));
                    loadOrReloadScripts(path);
                }
            }
        }
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

    ScriptContext ScriptHotReloadManager::makeContext(ECS::EntityID entityId, float deltaTime, ECS::Registry* registry) const
    {
        ScriptContext ctx{};
        ctx.entityId  = entityId;
        ctx.deltaTime = deltaTime;
        ctx.registry  = registry;

        return ctx;
    }

    void ScriptHotReloadManager::hotReload(const std::string& name,
                                           std::vector<panel::AttachedScript*>& activeScripts,
                                           std::function<ScriptContext(ECS::EntityID entityId)> makeCtx)
    {
        auto* script = getScript(name);
        if (!script) return;

        for (auto* attached : activeScripts) {
            if (attached->comp.scriptName != name || !attached->runtime.instance) continue;
            ScriptContext ctx = makeCtx(attached->runtime.loadedScriptIndex);
            callOnEnd(attached->comp, attached->runtime, ctx);
            destroyInstance(attached->comp, attached->runtime);
        }

        loadOrReloadScripts(script->sourcePath);

        for (auto* attached : activeScripts) {
            if (attached->comp.scriptName != name) continue;
            createInstance(attached->comp, attached->runtime);
            ScriptContext ctx = makeCtx(0);
            callOnStart(attached->comp, attached->runtime, ctx);
        }
    }

    #ifdef _WIN32
        std::string ScriptHotReloadManager::findVcvarsall()
        {
            // vswhere is always at this fixed path since VS 2017
            const char* vswhere = R"(%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe)";

            // Write vswhere output to a temp file then read it back
            const std::string tmpFile = std::string(std::getenv("TEMP") ? std::getenv("TEMP") : "C:\\Temp")
                                      + "\\gce_vswhere.txt";

            const std::string query = std::string("\"") + vswhere + "\""
                + " -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
                + " -property installationPath > \"" + tmpFile + "\" 2>nul";
            std::system(query.c_str());

            ///time for the command to happen
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::ifstream f(tmpFile);
            std::string installPath;
            std::getline(f, installPath);

            while (!installPath.empty() && (installPath.back() == '\r' ||
                                            installPath.back() == '\n' ||
                                            installPath.back() == ' '))
                installPath.pop_back();

            if (installPath.empty()) return {};
            return installPath + R"(\VC\Auxiliary\Build\vcvarsall.bat)";
        }
    #endif

    bool ScriptHotReloadManager::compileScript(const std::string& sourcePath, const std::string& outputPath)
    {
        std::string cmd;

        #ifdef _WIN32
        {
            static const std::string vcvarsall = findVcvarsall();

            std::string setup;
            if (!vcvarsall.empty())
            {
                setup = "\"" + vcvarsall + "\" x64 >nul 2>&1 && ";
            }
            else
            {
                Log::warn("vcvarsall.bat not found - cl.exe must already be on PATH");
                Log::warn("Make sure to have Visual Studio 17 or higher installed");
            }

            cmd = "cmd /C \"" + setup
                + "cl /nologo /O2 /LD /EHsc"
                + " /I\"" + m_includeDir + "\""
                + " \"" + sourcePath + "\""
                + " /Fe\"" + outputPath + "\""
                + " /Fd\"" + outputPath + ".pdb\""
                + " /Fo\"" + m_buildDir + "/\""
                + " /link /DLL /NOEXP /NOIMPLIB\"";
        }
        #elif defined(__APPLE__)
            cmd = "clang++ -std=c++20 -O2 -shared -fPIC -fvisibility=hidden "
                  "-I\"" + m_includeDir + "\" "
                  "\"" + sourcePath + "\" -o \"" + outputPath + "\"";
        #else
            cmd = "g++ -std=c++20 -O2 -shared -fPIC -fvisibility=hidden "
                  "-I\"" + m_includeDir + "\" "
                  "\"" + sourcePath + "\" -o \"" + outputPath + "\"";
        #endif

        Log::info(std::format("Compiling: {}", cmd));
        int result = std::system(cmd.c_str());

        if (result != 0)
        {
            Log::error(std::format("Cannot compile {}", sourcePath));
            return false;
        }

        // Clean up build artifacts left by cl.exe
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

    void ScriptHotReloadManager::loadOrReloadScripts(const std::string& sourcePath)
    {
        std::filesystem::path src(sourcePath);
        std::string name    = src.stem().string();
        std::string libPath = m_buildDir + "/" + name + lib_extension();

        auto currentWriteTime = std::filesystem::last_write_time(src);

        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end())
            m_scripts[it->second].lastWriteTime = currentWriteTime;

        // Unloads desired script before reloading it to memory
        if (it != m_nameToIndex.end())
        {
            auto& old = m_scripts[it->second];
            lib_unload(old.handle);
            old.handle = nullptr;
            old.valid  = false;
        }

        if (!compileScript(sourcePath, libPath))
        {
            // Show an error badge when the script fails to compile
            if (it == m_nameToIndex.end())
            {
                LoadedScript stub;
                stub.name          = name;
                stub.sourcePath    = sourcePath;
                stub.lastWriteTime = currentWriteTime;
                stub.valid         = false;
                m_nameToIndex[name] = m_scripts.size();
                m_scripts.push_back(std::move(stub));
            }
            Log::error(std::format("Failed to compile script {} at path \"{}\"", name, sourcePath));
            return;
        }

        LibHandle handle = lib_load(libPath.c_str());
        if (!handle)
        {
            Log::error(std::format("Failed to load {}", libPath));
            return;
        }

        LoadedScript script;
        script.name          = name;
        script.sourcePath    = sourcePath;
        script.libPath       = libPath;
        script.handle        = handle;
        script.create        = (FnCreate)   lib_symbol(handle, "scriptCreate");
        script.destroy       = (FnDestroy)  lib_symbol(handle, "scriptDestroy");
        script.onStart       = (FnOnStart)  lib_symbol(handle, "scriptOnStart");
        script.onUpdate      = (FnOnUpdate) lib_symbol(handle, "scriptOnUpdate");
        script.onEnd         = (FnOnEnd)    lib_symbol(handle, "scriptOnEnd");
        script.lastWriteTime = currentWriteTime;
        script.valid         = script.create && script.destroy
                            && script.onStart && script.onUpdate && script.onEnd;

        if (!script.valid)
            Log::error(std::format("Missing exports in {}", sourcePath));

        if (it != m_nameToIndex.end())
            m_scripts[it->second] = std::move(script);
        else
        {
            m_nameToIndex[name] = m_scripts.size();
            m_scripts.push_back(std::move(script));
        }

        Log::info(std::format("Script '{}' loaded successfully", name));
    }

} // namespace gcep
