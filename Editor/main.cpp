#include <iostream>

#include <Editor/Window/Window.hpp>
#include <Engine/Core/RHI/RenderWrapper/RHI_Vulkan.hpp>
#include <Editor/Window/ui_manager.hpp>

int main()
{
    gcep::RHI_Vulkan rhi;
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    try
    {
        rhi.setWindow(window.getGlfwWindow());
        rhi.initRHI();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    //gcep::UiManager uiManager(window.getGlfwWindow(), rhi.getInitInfo());
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        rhi.drawFrame();
        //uiManager.uiUpdate();
    }
    rhi.cleanup();
    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}