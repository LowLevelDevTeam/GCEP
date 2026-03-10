#pragma once

// Externals
#include <GLFW/glfw3.h>

// STL
#include <functional>
#include <optional>
#include <unordered_map>

namespace gcep
{

class InputSystem
{
public:
    InputSystem(GLFWwindow* window);

    void addTrackedKey(int key, std::function<void()> callback);
    void update();

private:
    GLFWwindow* windowRef;
    std::unordered_map<int,std::function<void()>> m_trackedKeys{};
};

} // Namespace gcep
