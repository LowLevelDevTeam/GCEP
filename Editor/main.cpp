#include <iostream>

#include <Editor/Window/Window.hpp>
#include <Engine/Core/RHI/RenderWrapper/RHI_Vulkan.hpp>
#include <Engine/Core/PhysicsWrapper/PhysicalWorld.hpp>

#include "Engine/Core/PhysicsWrapper/PhysicsManager.hpp"

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
    gcep::PhysicsSystem physicsSystem;
    physicsSystem->
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        rhi.drawFrame();
    }
    rhi.cleanup();
    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}
