#pragma once
#include <filesystem>
#include <imgui.h>
#include <vector>
#include <Editor/ProjectLoader/project_loader.hpp>


namespace gcep::editor
{

    class ContentBrowser
    {
    public:
        explicit ContentBrowser(const std::filesystem::path& defaultPath);

        void render();
    private:
        std::filesystem::path m_rootPath;
        std::filesystem::path m_currentPath;

        void folderTable();
        void refreshFolder();

    private:
        int folderSize;
        int horizontalSize;


        std::filesystem::path m_selectedPath;
        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> files;





    };
}

