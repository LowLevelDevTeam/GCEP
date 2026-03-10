#include "Inputs.hpp"

namespace gcep
{

InputSystem::InputSystem(GLFWwindow *window)
{
    windowRef = window;
}

void InputSystem::addTrackedKey(int key, std::function<void()> callback)
{
    m_trackedKeys.emplace(key, callback);
}

void InputSystem::update()
{
    for (auto& [key, function] : m_trackedKeys)
    {
        int state = glfwGetKey(windowRef, key);
        int mouseState = glfwGetMouseButton(windowRef, key);
        if (state == GLFW_PRESS)
        {
            function();
        }
        else if (mouseState == GLFW_PRESS)
        {
            function();
        }
    }
}

} // Namespace gcep
