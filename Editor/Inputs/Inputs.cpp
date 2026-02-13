//
// Created by cleme on 13/02/2026.
//

#include "Inputs.hpp"
#include <iostream>
namespace gcep
{
    void Inputs::addTrackedKey(int key, std::function<void()> callback)
    {
        m_trackedKeys.emplace(key, callback);
    }

    void Inputs::update(GLFWwindow* window)
    {
        for (auto& [key, function] : m_trackedKeys)
        {
            int state = glfwGetKey(window, key);
            if (state == GLFW_PRESS)
            {
                function();
            }
        }
    }
}
