#include <iostream>
#include "Window/Window.hpp"

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define EngineDir ../Engine
#include STR(EngineDir/Core/RHI/RenderWrapper/RHI_Vulkan.hpp)

void cleanup()
{
    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();
}

int main()
{
    gcep::RHI_Vulkan rhi;
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    try {
        rhi.setWindow(window.getGlfwWindow());
        rhi.initRHI();
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
    }
    cleanup();
    return 0;
}