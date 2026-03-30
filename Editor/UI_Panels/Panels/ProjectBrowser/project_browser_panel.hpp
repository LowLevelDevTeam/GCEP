#pragma once

// Internals
#include <Editor/UI_Panels/ipanel.hpp>

// STL
#include <filesystem>
#include <string>
#include <vector>

namespace gcep::panel
{
    /// @class ProjectBrowserPanel
    /// @brief Lists all .gcproj files found in the root directory and one level
    ///        of sub-directories. Double-clicking a project loads it and requests
    ///        an engine reload via EditorContext::reloadRequested.
    ///        Also embeds a ContentBrowser-style file navigator to locate projects anywhere.
    class ProjectBrowserPanel : public IPanel
    {
    public:
        /// @param rootPath  Directory to scan. Defaults to std::filesystem::current_path().
        explicit ProjectBrowserPanel(std::filesystem::path rootPath = std::filesystem::current_path());

        void draw() override;

    private:
        void refresh();

        // ── File browser ──────────────────────────────────────────────────────
        void refreshBrowser();
        void drawBrowser();
        void drawBrowserBackButton();
        void drawBrowserEntry(const std::filesystem::path& entry, bool isDir);

        struct ProjectEntry
        {
            std::string           name;
            std::filesystem::path path;
        };

        // Known-projects grid
        std::filesystem::path      m_rootPath;
        std::vector<ProjectEntry>  m_projects;
        int                        m_selectedIndex = -1;

        // File navigator
        std::filesystem::path              m_browserPath;
        std::vector<std::filesystem::path> m_browserDirs;
        std::vector<std::filesystem::path> m_browserProjFiles;
        std::filesystem::path              m_browserSelected;
    };

} // namespace gcep::panel
