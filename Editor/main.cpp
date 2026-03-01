#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <RHI/ObjParser/ObjParser.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>
#include <Engine/Core/Scripting/ScriptHost.hpp>
#include <Engine/Core/Scripting/ScriptAPI.hpp>
#include <Engine/Core/Scripting/ScriptComponent.hpp>
#include <Engine/Core/Scripting/ScriptSystem.hpp>
#include <Engine/Core/ECS/headers/registry.hpp>
#include <ScriptBuildConfig.hpp>

#include <filesystem>
#include <vector>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
#endif

namespace
{
    std::filesystem::path getExecutableDir()
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

    std::filesystem::path findScriptLibrary()
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

    std::filesystem::path findScriptSource()
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

    std::filesystem::path findBuildDir()
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
    std::string toShortPath(const std::string& path)
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
}

int main()
{
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    gcep::Inputs inputs;
    gcep::Camera camera(&inputs, &window);

    gcep::ECS::Registry registry;

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window.getGlfwWindow(), &fbWidth, &fbHeight);

    gcep::rhi::SwapchainDesc swapDesc{};
    swapDesc.nativeWindowHandle = window.getGlfwWindow();
    swapDesc.width              = static_cast<uint32_t>(fbWidth);
    swapDesc.height             = static_cast<uint32_t>(fbHeight);
    swapDesc.vsync              = true;

    std::unique_ptr<gcep::rhi::vulkan::VulkanRHI> rhi = std::make_unique<gcep::rhi::vulkan::VulkanRHI>(swapDesc);

    try
    {
        rhi->setWindow(window.getGlfwWindow());
        rhi->initRHI();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }

    auto& meshData = rhi->getMeshData();

    const std::filesystem::path scriptPath = findScriptLibrary();
    const gcep::ECS::EntityID scriptEntity = registry.createEntity();
    const std::uint32_t meshId = meshData.empty() ? UINT32_MAX : meshData.front().id;
    auto& scriptComponent = registry.addComponent<gcep::scripting::ScriptComponent>(scriptEntity, scriptPath, meshId);
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
        scriptComponent.host->startWatching();
    }

    gcep::scripting::ScriptSystem scriptSystem;
    scriptSystem.setMeshList(&meshData);

    gcep::UiManager uiManager(window.getGlfwWindow(), rhi->getInitInfo());
    uiManager.setMeshList(meshData);
    uiManager.setVieportResizeCallback(
        [&rhi](uint32_t width, uint32_t height)
        {
            rhi->requestOffscreenResize(width, height);
        }
    );
    uiManager.setScriptReloadCallback(
        [&scriptComponent]()
        {
            if (scriptComponent.host)
            {
                scriptComponent.host->requestReload();
            }
        }
    );

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        const double now = glfwGetTime();
        const double deltaSeconds = now - lastTime;
        lastTime = now;

        glfwPollEvents();
        inputs.update(window.getGlfwWindow());

        scriptSystem.update(registry, deltaSeconds);

        rhi->processPendingOffscreenResize();
        uiManager.uiUpdate(
            rhi->getImGuiTextureDescriptor(),
            &camera,
            rhi->getDrawCount()
        );
        rhi->updateCameraUBO(camera.update(uiManager.getViewportSize().x / uiManager.getViewportSize().y, uiManager.camSpeed));
        rhi->updateSceneUBO(uiManager.getSceneInfos(), camera.position);

        rhi->drawFrame();
    }
    rhi->cleanup();
    rhi.reset();

    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}
