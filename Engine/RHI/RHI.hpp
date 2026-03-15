#pragma once

// Forward declaration
struct GLFWwindow;

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
        virtual void cleanup() = 0;
    };
} // namespace gcep
