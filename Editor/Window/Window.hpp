#pragma once

// Externals
#include <GLFW/glfw3.h>

namespace gcep
{

class Window
{
public:
    static Window& getInstance()
    {
        static Window instance;
        return instance;
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void initWindow();

public:
    GLFWwindow* getGlfwWindow();
    bool shouldClose();

private:
    Window() = default;
    ~Window();

    void centerWindow();

private:
    GLFWwindow* m_window;
};

} // Namespace gcep