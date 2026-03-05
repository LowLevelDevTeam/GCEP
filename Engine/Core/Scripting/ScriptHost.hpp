#pragma once

#include "ScriptAPI.hpp"
#include "SharedLibrary.hpp"

#include <Engine/Core/ECS/headers/entity_component.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

namespace gcep::ECS
{
    class Registry;
}

namespace gcep::scripting
{
    class ScriptHost
    {
    public:
        explicit ScriptHost(std::filesystem::path sourceLibraryPath);
        ~ScriptHost();

        ScriptHost(const ScriptHost&) = delete;
        ScriptHost& operator=(const ScriptHost&) = delete;

        [[nodiscard]] bool load(std::string* error = nullptr);
        void unload();

        /// Update a specific plugin by name with a caller-owned context
        void updatePlugin(const std::string& scriptName, ScriptContext* context);

        /// Get a plugin entry by name (nullptr if not found)
        [[nodiscard]] ScriptRegistryEntry* getPluginByName(const std::string& name) const;
        /// Get the number of registered scripts in the loaded DLL
        [[nodiscard]] std::size_t getScriptCount() const;

        void requestReload();
        void startWatching(std::chrono::milliseconds interval = std::chrono::milliseconds(250));
        void stopWatching();
        void setSourceFilePath(const std::filesystem::path& sourcePath);
        void setBuildCommand(std::string commandLine);

        [[nodiscard]] const std::filesystem::path& getSourcePath() const;

    private:
        bool reloadIfNeeded();
        [[nodiscard]] bool loadInternal(std::string* error);
        [[nodiscard]] bool shouldReload() const;
        void notifyLoad();
        void notifyUnload();
        void runWatcher(std::chrono::milliseconds interval);
        bool maybeBuildFromSource();
        bool waitForLibraryUpdate(std::chrono::milliseconds timeout);
        bool waitForLibraryStable(std::chrono::milliseconds timeout);

        static void defaultLog(const char* message);

    private:
        std::filesystem::path m_sourcePath;
        std::filesystem::path m_loadedPath;
        std::filesystem::path m_sourceFilePath;
        std::filesystem::file_time_type m_lastWriteTime{};
        std::filesystem::file_time_type m_lastSourceWriteTime{};
        SharedLibrary m_library;

        // Multi-plugin registry function pointers (resolved from DLL)
        GetScriptCountFn m_getCount = nullptr;
        GetScriptEntryFn m_getEntry = nullptr;
        GetScriptEntryByNameFn m_getEntryByName = nullptr;

        std::atomic<bool> m_reloadRequested{false};
        unsigned long long m_reloadCounter = 0;
        std::unique_ptr<ScriptContext> m_context;
        std::string m_buildCommand;

        std::atomic<bool> m_watcherRunning{false};
        std::thread m_watcherThread;
        mutable std::mutex m_timestampMutex;
        std::mutex m_buildMutex;
        std::atomic<bool> m_buildInProgress{false};

        int m_copyRetryCount = 10;
        std::chrono::milliseconds m_copyRetryDelay{50};
    };
}
