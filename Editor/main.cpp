#include <iostream>
#include "Window/Window.hpp"

int main() {
    std::cout << "Hello, World!" << std::endl;
    Window& window = Window::getInstance();
    window.initWindow();
    return 0;
}