#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <RHI/ObjParser/ObjParser.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>

int main()
{
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    gcep::Inputs inputs;
    gcep::Camera camera(&inputs, &window);

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

    gcep::UiManager uiManager(window.getGlfwWindow(), rhi->getInitInfo());
    uiManager.setMeshList(rhi->getMeshData());
    uiManager.setVieportResizeCallback(
        [&rhi](uint32_t width, uint32_t height)
        {
            rhi->requestOffscreenResize(width, height);
        }
    );
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        inputs.update(window.getGlfwWindow());

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
