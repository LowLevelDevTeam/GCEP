#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/UIManager/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <RHI/ObjParser/ObjParser.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>
#include <Engine/Core/Scripting/ScriptHost.hpp>
#include <Engine/Core/Scripting/ScriptSystem.hpp>
#include <Engine/Core/ECS/headers/registry.hpp>

#include <Engine/RHI/Vulkan/VulkanRHI.hpp>
#include <Log/Log.hpp>

int main()
{
    using namespace gcep;
    using VulkanRHI = rhi::vulkan::VulkanRHI;

    Log::initialize("GC Engine Paris", "crash.log");
    Window& window = Window::getInstance();
    window.initWindow();
    InputSystem inputSystem(window.getGlfwWindow());
    Camera camera(&inputSystem, &window);

    gcep::ECS::Registry registry;

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window.getGlfwWindow(), &fbWidth, &fbHeight);

    rhi::SwapchainDesc swapDesc{};
    swapDesc.nativeWindowHandle = window.getGlfwWindow();
    swapDesc.width              = static_cast<uint32_t>(fbWidth);
    swapDesc.height             = static_cast<uint32_t>(fbHeight);
    swapDesc.vsync              = true;

    std::unique_ptr<VulkanRHI> rhi = std::make_unique<VulkanRHI>(swapDesc);


    bool hasToReload = false;
    try
    {
        rhi->setWindow(window.getGlfwWindow());
        rhi->initRHI();
    }
    catch (std::exception& e)
    {
        Log::handleException(e);
        std::cerr << e.what() << std::endl;
        return 1;
    }

    auto& meshData = rhi->getMeshData();

    // Run at game startup
    gcep::scripting::ScriptSystem scriptSystem;
    scriptSystem.init(&registry, meshData);
    scriptSystem.setMeshList(&meshData);

    gcep::UiManager uiManager(window.getGlfwWindow(), rhi->getInitInfo());
    uiManager.setMeshList(meshData);
    uiManager.setVieportResizeCallback(
        [&rhi](uint32_t width, uint32_t height)
        {
            rhi->requestOffscreenResize(width, height);
        }
    );

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    UiManager uiManager(window.getGlfwWindow(), rhi->getUIInitInfo());
    uiManager.setCamera(&camera);
    uiManager.setInfos(rhi->getInitInfos());

    while (!window.shouldClose())
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
            rhi->getDrawCount(),
            &hasToReload
        );

        if (hasToReload)
            std::cout << "Reloading..." << std::endl;
        rhi->updateCameraUBO(camera.update(uiManager.getViewportSize().x / uiManager.getViewportSize().y, uiManager.camSpeed));
        rhi->updateSceneUBO(uiManager.getSceneInfos(), camera.position);

        inputSystem.update();
        uiManager.uiUpdate();
        rhi->drawFrame();
    }
    Log::info("Window closed");
    rhi->cleanup();
    rhi.reset();

    return 0;
}
