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


        void folderTable();
        void refreshFolder();
        void renderDirectoryEntry(const std::filesystem::path& entry);
        void renderFileEntry(const std::filesystem::path& entry);
        void renderCenteredIcon(const std::string& icon);
        void renderCenteredLabel(const std::string& label, float cellWidth = 64.0f);
        void renderDragSource(const std::filesystem::path& entry);
        bool renderDropTarget(const std::filesystem::path& destination);
        void renderBackButton();

    private:
        int folderSize;
        std::filesystem::path m_rootPath;
        std::filesystem::path m_currentPath;
        std::filesystem::path m_selectedPath;
        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> files;
    };
}

