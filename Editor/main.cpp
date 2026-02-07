#include <iostream>

#include <Editor/Window/Window.hpp>
#include <Engine/Core/RHI/RenderWrapper/RHI_Vulkan.hpp>

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
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        rhi.drawFrame();
    }
    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}