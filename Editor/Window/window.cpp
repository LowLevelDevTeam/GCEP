#include "window.hpp"
#include <Log/Log.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace gcep
{

void Window::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const auto monitor          = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode     = glfwGetVideoMode(monitor);

    const int WINDOW_WIDTH  = mode->width  / 2;
    const int WINDOW_HEIGHT = mode->height / 2;

    m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "GC Engine", nullptr, nullptr);
    centerWindow();

    Log::info(std::format("Window initialized successfully ({}x{})", WINDOW_WIDTH, WINDOW_HEIGHT));
}

void Window::centerWindow()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor)
        return;

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode)
        return;

    int monitorX, monitorY;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    int windowWidth, windowHeight;
    glfwGetWindowSize(m_window, &windowWidth, &windowHeight);
    glfwSetWindowPos(
        m_window,
        monitorX + (mode->width  - windowWidth)  / 2,
        monitorY + (mode->height - windowHeight) / 2
    );
}

GLFWwindow* Window::getGlfwWindow()
{
    return m_window;
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(m_window);
}

void Window::destroy()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
    Log::info("Window destroyed");
}

Window::~Window()
{
    destroy();
}

} // namespace gcep
