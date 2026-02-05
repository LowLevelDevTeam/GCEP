#pragma once

#include <GLFW/glfw3.h>

class Window {
public:
    static Window& getInstance() {
        static Window instance;
        return instance;
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void initWindow();

private:
    Window() = default;
    ~Window() = default;

private:
    GLFWwindow *window_;
};