#pragma once
#include <functional>
#include <optional>
#include <unordered_map>
#include <GLFW/glfw3.h>

namespace gcep
{
    class Inputs
    {
    public:
        Inputs () = default;

        void addTrackedKey (int key, std::function<void()> callback);

        void update (GLFWwindow* window);
    private:

        std::unordered_map<int,std::function<void()>> m_trackedKeys{};
    };

}
