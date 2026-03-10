#pragma once
#include <filesystem>
#include <string>
#include <GLFW/glfw3.h>

namespace gcep { struct ProjectInfo; }

namespace pl
{
    struct ProjectInfo
    {
        std::filesystem::path projectPath;
        std::filesystem::path startScene;
        std::string           projectName;
    };

    class project_loader
    {
    public:
        explicit project_loader(GLFWwindow* window);
        ~project_loader() = default;

        void drawUI(bool& stillSelecting);

        [[nodiscard]] ProjectInfo getProjectInfo() const { return m_info; }

    private:
        GLFWwindow*  m_window = nullptr;
        ProjectInfo  m_info;
        char         m_projectName[256] = {};
    };
}