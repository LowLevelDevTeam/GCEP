#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <RHI/ObjParser/ObjParser.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>
#include <Engine/Core/Scripting/ScriptHost.hpp>
#include <Engine/Core/Scripting/ScriptSystem.hpp>
#include <Engine/Core/ECS/headers/registry.hpp>


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


    bool hasToReload = false;
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

        rhi->drawFrame();
    }
    rhi->cleanup();
    rhi.reset();

    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}
