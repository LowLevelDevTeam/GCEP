#include "ScriptHost.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace gcep::scripting
{
    ScriptHost::ScriptHost(std::filesystem::path sourceLibraryPath)
        : m_sourcePath(std::move(sourceLibraryPath)), m_context(std::make_unique<ScriptContext>())
    {
        m_context->log = &ScriptHost::defaultLog;
    }

    ScriptHost::~ScriptHost()
    {
        stopWatching();
        unload();
    }

    bool ScriptHost::load(std::string* error)
    {
        return loadInternal(error);
    }

    void ScriptHost::unload()
    {
        notifyUnload();

        if (m_destroyPlugin && m_plugin)
        {
            m_destroyPlugin(m_plugin);
        }
        m_plugin = nullptr;
        m_destroyPlugin = nullptr;

        m_library.unload();

        if (!m_loadedPath.empty())
        {
            std::error_code removeError;
            std::filesystem::remove(m_loadedPath, removeError);
            m_loadedPath.clear();
        }
    }

    void ScriptHost::update(double deltaSeconds)
    {
        m_context->deltaSeconds = deltaSeconds;

        if (reloadIfNeeded())
        {
            return;
        }

        if (m_plugin && m_plugin->onUpdate)
        {
            m_plugin->onUpdate(m_context.get(), m_plugin->state);
        }
    }

    void ScriptHost::requestReload()
    {
        m_reloadRequested.store(true, std::memory_order_relaxed);
    }

    void ScriptHost::startWatching(std::chrono::milliseconds interval)
    {
        if (m_watcherRunning.exchange(true))
        {
            return;
        }

        m_watcherThread = std::thread(&ScriptHost::runWatcher, this, interval);
    }

    void ScriptHost::stopWatching()
    {
        if (!m_watcherRunning.exchange(false))
        {
            return;
        }

        if (m_watcherThread.joinable())
        {
            m_watcherThread.join();
        }
    }

    const std::filesystem::path& ScriptHost::getSourcePath() const
    {
        return m_sourcePath;
    }

    bool ScriptHost::reloadIfNeeded()
    {
        if (m_buildInProgress.load(std::memory_order_relaxed))
        {
            return false;
        }
        if (!shouldReload())
        {
            return false;
        }

        if (!waitForLibraryStable(std::chrono::milliseconds(1500)))
        {
            defaultLog("DLL not stable yet, retrying later");
            return false;
        }

        defaultLog("Reloading script...");
        std::string error;
        unload();
        if (!loadInternal(&error))
        {
            if (!error.empty())
            {
                defaultLog(error.c_str());
            }
            return false;
        }
        return true;
    }

    bool ScriptHost::loadInternal(std::string* error)
    {
        m_reloadRequested.store(false, std::memory_order_relaxed);

        std::error_code fileError;
        if (!std::filesystem::exists(m_sourcePath, fileError))
        {
            if (error)
            {
                *error = "Script library not found: " + m_sourcePath.string();
            }
            return false;
        }

        const auto writeTime = std::filesystem::last_write_time(m_sourcePath, fileError);
        if (fileError)
        {
            if (error)
            {
                *error = "Failed to read script timestamp";
            }
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_timestampMutex);
            m_lastWriteTime = writeTime;
        }

        const std::filesystem::path parent = m_sourcePath.parent_path();
        const std::string stem = m_sourcePath.stem().string();
        const std::string extension = m_sourcePath.extension().string();

        m_loadedPath = parent / (stem + "_hot_" + std::to_string(++m_reloadCounter) + extension);
        std::error_code copyError;
        for (int attempt = 0; attempt < m_copyRetryCount; ++attempt)
        {
            std::filesystem::copy_file(m_sourcePath, m_loadedPath, std::filesystem::copy_options::overwrite_existing, copyError);
            if (!copyError)
            {
                break;
            }
            std::this_thread::sleep_for(m_copyRetryDelay);
        }
        if (copyError)
        {
            if (error)
            {
                *error = "Failed to copy script library: " + copyError.message() + " (" + m_sourcePath.string() + " -> " + m_loadedPath.string() + ")";
            }
            return false;
        }

        if (!m_library.load(m_loadedPath, error))
        {
            return false;
        }

        std::string symbolError;
        auto createFn = reinterpret_cast<CreateScriptPluginFn>(m_library.getSymbol(kCreateSymbol, &symbolError));
        if (!createFn)
        {
            if (error)
            {
                *error = symbolError.empty() ? "Missing create symbol" : symbolError;
            }
            m_library.unload();
            return false;
        }

        m_destroyPlugin = reinterpret_cast<DestroyScriptPluginFn>(m_library.getSymbol(kDestroySymbol, &symbolError));
        if (!m_destroyPlugin)
        {
            if (error)
            {
                *error = symbolError.empty() ? "Missing destroy symbol" : symbolError;
            }
            m_library.unload();
            return false;
        }

        m_plugin = createFn();
        if (!m_plugin)
        {
            if (error)
            {
                *error = "Create plugin returned null";
            }
            m_library.unload();
            return false;
        }

        notifyLoad();

        return true;
    }

    bool ScriptHost::shouldReload() const
    {
        if (m_reloadRequested.load(std::memory_order_relaxed))
        {
            return true;
        }

        std::error_code fileError;
        if (!std::filesystem::exists(m_sourcePath, fileError))
        {
            return false;
        }

        const auto timestamp = std::filesystem::last_write_time(m_sourcePath, fileError);
        if (fileError)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_timestampMutex);
        return timestamp != m_lastWriteTime;
    }

    void ScriptHost::notifyLoad()
    {
        if (m_plugin && m_plugin->onLoad)
        {
            m_plugin->onLoad(m_context.get(), &m_plugin->state);
        }
    }

    void ScriptHost::notifyUnload()
    {
        if (m_plugin && m_plugin->onUnload)
        {
            m_plugin->onUnload(m_context.get(), m_plugin->state);
        }
    }

    void ScriptHost::runWatcher(std::chrono::milliseconds interval)
    {
        while (m_watcherRunning.load(std::memory_order_relaxed))
        {
            maybeBuildFromSource();
            if (shouldReload())
            {
                requestReload();
            }
            std::this_thread::sleep_for(interval);
        }
    }

    bool ScriptHost::maybeBuildFromSource()
    {
        if (m_sourceFilePath.empty() || m_buildCommand.empty())
        {
            return false;
        }

        std::lock_guard<std::mutex> buildLock(m_buildMutex);

        std::error_code error;
        if (!std::filesystem::exists(m_sourceFilePath, error))
        {
            return false;
        }

        const auto timestamp = std::filesystem::last_write_time(m_sourceFilePath, error);
        if (error)
        {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_timestampMutex);
            if (timestamp == m_lastSourceWriteTime)
            {
                return false;
            }
            m_lastSourceWriteTime = timestamp;
        }

        defaultLog("Script source changed, building...");
        defaultLog(m_buildCommand.c_str());
        m_buildInProgress.store(true, std::memory_order_relaxed);
        const int result = std::system(m_buildCommand.c_str());
        m_buildInProgress.store(false, std::memory_order_relaxed);
        if (result != 0)
        {
            defaultLog("Script build failed");
            return false;
        }

        if (!waitForLibraryUpdate(std::chrono::milliseconds(2000)))
        {
            defaultLog("Script build finished but DLL not updated yet");
        }

        requestReload();
        return true;
    }

    bool ScriptHost::waitForLibraryUpdate(std::chrono::milliseconds timeout)
    {
        if (m_sourcePath.empty())
        {
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        std::error_code error;
        std::filesystem::file_time_type lastWrite;
        {
            std::lock_guard<std::mutex> lock(m_timestampMutex);
            lastWrite = m_lastWriteTime;
        }

        while (std::chrono::steady_clock::now() - start < timeout)
        {
            if (!std::filesystem::exists(m_sourcePath, error))
            {
                return false;
            }
            const auto current = std::filesystem::last_write_time(m_sourcePath, error);
            if (!error && current != lastWrite)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        return false;
    }

    bool ScriptHost::waitForLibraryStable(std::chrono::milliseconds timeout)
    {
        if (m_sourcePath.empty())
        {
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        std::error_code error;
        std::uintmax_t lastSize = 0;
        std::filesystem::file_time_type lastWrite;

        while (std::chrono::steady_clock::now() - start < timeout)
        {
            if (!std::filesystem::exists(m_sourcePath, error))
            {
                return false;
            }
            lastSize = std::filesystem::file_size(m_sourcePath, error);
            if (error)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            lastWrite = std::filesystem::last_write_time(m_sourcePath, error);
            if (error)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            const auto sizeNow = std::filesystem::file_size(m_sourcePath, error);
            if (error)
            {
                continue;
            }
            const auto writeNow = std::filesystem::last_write_time(m_sourcePath, error);
            if (!error && sizeNow == lastSize && writeNow == lastWrite)
            {
                return true;
            }
        }

        return false;
    }

    void ScriptHost::defaultLog(const char* message)
    {
        if (!message)
        {
            return;
        }
        std::cout << "[Script] " << message << std::endl;
    }

    void ScriptHost::setSourceFilePath(const std::filesystem::path& sourcePath)
    {
        std::lock_guard<std::mutex> lock(m_timestampMutex);
        m_sourceFilePath = sourcePath;
        std::error_code error;
        if (!m_sourceFilePath.empty() && std::filesystem::exists(m_sourceFilePath, error))
        {
            m_lastSourceWriteTime = std::filesystem::last_write_time(m_sourceFilePath, error);
        }
    }

    void ScriptHost::setBuildCommand(std::string commandLine)
    {
        m_buildCommand = std::move(commandLine);
    }

    void ScriptHost::setEcsContext(ECS::Registry* registry, ECS::EntityID entity)
    {
        m_context->registry = registry;
        m_context->entity = entity;
    }

    void ScriptHost::setMeshContext(rhi::vulkan::Mesh* mesh)
    {
        m_context->mesh = mesh;
    }
}
