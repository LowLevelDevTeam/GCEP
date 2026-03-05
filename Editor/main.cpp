#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/UIManager/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <Engine/RHI/Vulkan/VulkanRHI.hpp>
#include <Log/Log.hpp>

int main()
{
    using namespace gcep;
    using VulkanRHI = rhi::vulkan::VulkanRHI;

    static bool isRunning = true;
    static bool reloadRequested = false;
    while(isRunning)
    {
        if(!reloadRequested)
        {
            Log::initialize("GC Engine Paris", "crash.log");
        }
        else
        {
            Log::info("--- Engine reloaded ---\n");
        }
        reloadRequested = false;

        Window &window = Window::getInstance();
        window.initWindow();
        InputSystem inputSystem(window.getGlfwWindow());
        Camera camera(&inputSystem, &window);

        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window.getGlfwWindow(), &fbWidth, &fbHeight);

        rhi::SwapchainDesc swapDesc{};
        swapDesc.nativeWindowHandle = window.getGlfwWindow();
        swapDesc.width = static_cast<uint32_t>(fbWidth);
        swapDesc.height = static_cast<uint32_t>(fbHeight);
        swapDesc.vsync = true;

        std::unique_ptr<VulkanRHI> rhi = std::make_unique<VulkanRHI>(swapDesc);

        try
        {
            rhi->setWindow(window.getGlfwWindow());
            rhi->initRHI();
        }
        catch (std::exception &e)
        {
            Log::handleException(e);
            std::cerr << e.what() << std::endl;
            return 1;
        }

        UiManager uiManager(window.getGlfwWindow(), rhi->getUIInitInfo(), reloadRequested, isRunning);
        uiManager.setCamera(&camera);
        uiManager.setInfos(rhi->getInitInfos());

        while (!window.shouldClose() && !reloadRequested)
        {
            glfwPollEvents();
            inputSystem.update();
            uiManager.uiUpdate();
            rhi->drawFrame();
        }
        Log::info("Window closed");
        rhi->cleanup();
        rhi.reset();
        window.destroy();

        if (!reloadRequested)
        {
            isRunning = false;
        }
    }

    return 0;
}
