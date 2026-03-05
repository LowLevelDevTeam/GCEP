#include "ScriptSystem.hpp"

#include <iostream>

#include "ScriptAPI.hpp"
#include "ScriptComponent.hpp"
#include "PhysicsWrapper/physics_system.hpp"

namespace gcep::scripting
{
    void ScriptSystem::init(ECS::Registry* registry, const std::vector<rhi::vulkan::Mesh>& meshDataVector)
    {
        // Build Scripts
        std::string configureCommand =
             "cmake -S \"" + m_sourcePath +
             "\" -B \"" + m_buildPath +
             "\" -G Ninja";

        int configureResult = std::system(configureCommand.c_str());
        if (configureResult != 0)
        {
            std::cerr << "[ScriptSystem] CMake configure failed\n";
            return;
        }

        std::string buildCommand =
            "cmake --build \"" + m_buildPath + "\"";

        int buildResult = std::system(buildCommand.c_str());
        if (buildResult != 0)
        {
            std::cerr << "[ScriptSystem] CMake build failed\n";
        }
        //========================================

        m_registry = registry;

        const std::filesystem::path scriptPath = findScriptLibrary();
        const gcep::ECS::EntityID scriptEntity = m_registry->createEntity();

        for (auto& mesh : meshDataVector)
        {
            const std::uint32_t meshId = mesh.id;

            auto& scriptComponent = m_registry->addComponent<gcep::scripting::ScriptComponent>(scriptEntity, scriptPath, meshId);

            std::string scriptError;
            if (!scriptPath.empty())
            {
                std::cout << "Script library found: " << scriptPath.string() << std::endl;
                if (scriptComponent.host && !scriptComponent.host->load(&scriptError))
                {
                    std::cout << "Script load failed: " << scriptError << std::endl;
                }
            }
            else
            {
                const std::filesystem::path expected = getExecutableDir() / "Scripts" / (std::string("GCEngineScripts") + gcep::scripting::kSharedLibraryExtension);
                std::cout << "Script library not found. Expected: " << expected.string() << std::endl;
            }

            const std::filesystem::path sourcePath = findScriptSource();
            if (!sourcePath.empty())
            {
                if (scriptComponent.host)
                {
                    scriptComponent.host->setSourceFilePath(sourcePath);
                }
            }

            std::filesystem::path buildDir;
            if (!scriptPath.empty())
            {
                buildDir = scriptPath.parent_path().parent_path();
            }
            if (buildDir.empty())
            {
                buildDir = findBuildDir();
            }

            if (!buildDir.empty())
            {
#if defined(_WIN32)
                const std::string cmakePath = toShortPath(GCE_CMAKE_COMMAND);
                const std::string buildCommand = cmakePath + " --build \"" + buildDir.string() + "\" --target GCEngineScripts";
#else
                const std::string buildCommand = std::string("cmake") + " --build \"" + buildDir.string() + "\" --target GCEngineScripts";
#endif
                if (scriptComponent.host)
                {
                    scriptComponent.host->setBuildCommand(buildCommand);
                }
            }

            if (scriptComponent.host)
            {
                // No hot reload scriptComponent.host->startWatching();
            }
        }
    }

    void ScriptSystem::setMeshList(std::vector<rhi::vulkan::Mesh>* meshes)
    {
        m_meshes = meshes;
    }

    void ScriptSystem::update(ECS::Registry* registry, double deltaSeconds)
    {
        for (auto entity : registry->view<ScriptComponent>())
        {

            auto& component = registry->getComponent<ScriptComponent>(entity);
            if (!component.host)
            {
                continue;
            }
            component.host->setEcsContext(registry, entity);
            component.host->setMeshContext(findMesh(component.meshId));
            component.host->setPhysicsContext(&gcep::PhysicsSystem::getInstance());
            component.host->update(deltaSeconds);
        }
    }

    ScriptSystem& ScriptSystem::getInstance()
    {
        static ScriptSystem s_instance;
        return s_instance;
    }

    std::filesystem::path ScriptSystem::getExecutableDir()
    {
        #if defined(_WIN32)
                char buffer[MAX_PATH] = {};
                const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
                if (len > 0)
                {
                    return std::filesystem::path(std::string(buffer, len)).parent_path();
                }
        #elif defined(__APPLE__)
                uint32_t size = 0;
                _NSGetExecutablePath(nullptr, &size);
                std::string buffer(size, '\0');
                if (_NSGetExecutablePath(buffer.data(), &size) == 0)
                {
                    return std::filesystem::path(buffer).parent_path();
                }
        #else
                std::string buffer(1024, '\0');
                const ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size());
                if (len > 0)
                {
                    return std::filesystem::path(std::string(buffer.data(), static_cast<size_t>(len))).parent_path();
                }
        #endif
                return std::filesystem::current_path();
    }

    std::filesystem::path ScriptSystem::findScriptLibrary()
    {
        const std::string fileName = std::string("GCEngineScripts") + gcep::scripting::kSharedLibraryExtension;
        const std::filesystem::path exeDir = getExecutableDir();
        const std::filesystem::path cwd = std::filesystem::current_path();
        const std::filesystem::path parent = cwd.parent_path();

        const std::vector<std::filesystem::path> baseCandidates = {
            exeDir,
            exeDir.parent_path(),
            cwd,
            parent,
            cwd / "cmake-build-debug",
            cwd / "cmake-build-release",
            parent / "cmake-build-debug",
            parent / "cmake-build-release"
        };

        for (const auto& base : baseCandidates)
        {
            if (base.empty())
            {
                continue;
            }
            const auto candidate = base / "Scripts" / fileName;
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        return {};
    }

    std::filesystem::path ScriptSystem::findScriptSource()
    {
        const std::filesystem::path exeDir = getExecutableDir();
        const std::filesystem::path cwd = std::filesystem::current_path();
        const std::filesystem::path parent = cwd.parent_path();

        const std::vector<std::filesystem::path> baseCandidates = {
            exeDir.parent_path(),
            cwd,
            parent
        };

        for (const auto& base : baseCandidates)
        {
            if (base.empty())
            {
                continue;
            }
            const auto candidate = base / "Scripts" / "SampleScript.cpp";
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        return {};
    }

    std::filesystem::path ScriptSystem::findBuildDir()
    {
        if (std::filesystem::exists(GCE_CMAKE_BINARY_DIR))
        {
            return std::filesystem::path(GCE_CMAKE_BINARY_DIR);
        }
        const std::filesystem::path exeDir = getExecutableDir();
        const std::filesystem::path cwd = std::filesystem::current_path();
        const std::filesystem::path parent = cwd.parent_path();

        const std::vector<std::filesystem::path> candidates = {
            exeDir,
            exeDir.parent_path(),
            cwd,
            parent,
            cwd / "cmake-build-debug",
            cwd / "cmake-build-release",
            parent / "cmake-build-debug",
            parent / "cmake-build-release"
        };

        for (const auto& base : candidates)
        {
            if (base.empty())
            {
                continue;
            }
            if (std::filesystem::exists(base / "build.ninja") || std::filesystem::exists(base / "CMakeCache.txt"))
            {
                return base;
            }
        }

        return {};
    }

#if defined(_WIN32)
    std::string ScriptSystem::toShortPath(const std::string& path)
    {
        if (path.empty())
        {
            return path;
        }
        char buffer[MAX_PATH] = {};
        const DWORD len = GetShortPathNameA(path.c_str(), buffer, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
        {
            return path;
        }
        return std::string(buffer, len);
    }
#endif

    rhi::vulkan::Mesh* ScriptSystem::findMesh(std::uint32_t meshId) const
    {
        if (!m_meshes)
        {
            return nullptr;
        }
        for (auto& mesh : *m_meshes)
        {
            if (mesh.id == meshId)
            {
                return &mesh;
            }
        }
        return nullptr;
    }
}
