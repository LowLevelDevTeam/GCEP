#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace gcep
{

class RHI
{
    public:
    virtual ~RHI() = default;

    protected:
    virtual void initRHI() = 0;
    virtual void drawFrame() = 0;
    virtual void setWindow(GLFWwindow* window) = 0;
};

} // Namespace gcep