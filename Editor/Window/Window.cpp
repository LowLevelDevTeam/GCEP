#include "Window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace gcep
{

void Window::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const auto monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    const int WINDOW_WIDTH = mode->width / 2;
    const int WINDOW_HEIGHT = mode->height / 2;

    m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "GC Engine", nullptr, nullptr);
}

GLFWwindow* Window::getGlfwWindow()
{
    return m_window;
}

} // Namespace gcep