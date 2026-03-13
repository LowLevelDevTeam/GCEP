#pragma once
#include <filesystem>
#include <string>
#include <GLFW/glfw3.h>
#include <Externals/rapidjson/document.h>
#include <Externals/rapidjson/prettywriter.h>
#include <Externals/rapidjson/stringbuffer.h>
#include <Externals/rapidjson/istreamwrapper.h>


namespace gcep { struct ProjectInfo; }

namespace pl
{

    struct ProjectSettings
    {
        bool  vsync             = false;

        float clearColor[4]     = { 0.1f, 0.1f, 0.1f, 1.0f };
        float ambientColor[3]   = { 0.2f, 0.2f, 0.2f };
        float lightColor[3]     = { 1.0f, 1.0f, 1.0f };
        float lightDirection[3] = { 0.0f,-1.0f, 0.0f };

        float cameraSpeed       = 5.0f;

        float gridCellSize      = 1.0f;
        float gridThickEvery    = 5.0f;
        float gridFadeDistance  = 50.0f;
        float gridLineWidth     = 1.0f;

        float taaBlendAlpha     = 0.10f;
    };

    struct ProjectInfo
    {
        std::filesystem::path projectPath;
        std::filesystem::path contentPath;
        std::filesystem::path startScene;
        std::string           projectName;
        std::string           version   = "1.0";
        std::string           createdAt;
        ProjectSettings       settings;
    };

    class project_loader
    {
    public:

        static project_loader& instance();

        void init(GLFWwindow* window);

        void update(double deltaTime);

        void saveProject();

        void markDirty() { m_dirty = true; }

        void drawUI(bool& stillSelecting);

        ProjectSettings& getSettings() { return m_info.settings; }

        [[nodiscard]] ProjectInfo getProjectInfo() const { return m_info; }

        project_loader(const project_loader&)            = delete;
        project_loader& operator=(const project_loader&) = delete;

    private:
        project_loader() = default;

        void loadProject(const std::filesystem::path& projFile);
        void writeProjectFile();

        void writeDefaultSceneFile();

        std::string nowISO8601();

        GLFWwindow*  m_window = nullptr;
        ProjectInfo  m_info;
        char         m_projectName[256] = {};

        bool   m_dirty              = false;
        double m_dirtyTimer         = 0.0;
        double m_dirtyDebounce      = 2.0;
        double m_autoSaveTimer      = 0.0;
        double m_autoSaveInterval   = 300.0;
    };
}