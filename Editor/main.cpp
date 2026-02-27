#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>

int main()
{
    using namespace gcep;
    using VulkanRHI = gcep::rhi::vulkan::VulkanRHI;

    Window& window = Window::getInstance();
    window.initWindow();
    InputSystem inputSystem(window.getGlfwWindow());
    Camera camera(&inputSystem, &window);

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window.getGlfwWindow(), &fbWidth, &fbHeight);

    rhi::SwapchainDesc swapDesc{};
    swapDesc.nativeWindowHandle = window.getGlfwWindow();
    swapDesc.width              = static_cast<uint32_t>(fbWidth);
    swapDesc.height             = static_cast<uint32_t>(fbHeight);
    swapDesc.vsync              = true;

    std::unique_ptr<VulkanRHI> rhi = std::make_unique<VulkanRHI>(swapDesc);

    try
    {
        rhi->setWindow(window.getGlfwWindow());
        rhi->initRHI();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    UiManager uiManager(window.getGlfwWindow(), rhi->getUIInitInfo());
    uiManager.setCamera(&camera);
    uiManager.setInfos(rhi->getInitInfos());

    while (!window.shouldClose())
    {
        glfwPollEvents();
        inputSystem.update();
        uiManager.uiUpdate();
        rhi->drawFrame();
    }
    rhi->cleanup();
    rhi.reset();

    return 0;
}
