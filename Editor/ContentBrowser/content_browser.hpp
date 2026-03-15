#pragma once

/// @file content_browser.hpp
/// @brief Dockable file-system browser panel for the editor.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Internals
#include <Editor/ProjectLoader/project_loader.hpp>

// Externals
#include <imgui.h>

// STL
#include <filesystem>
#include <vector>

namespace gcep::editor
{
    /// @class ContentBrowser
    /// @brief Dear ImGui panel that lets the user navigate the project's content directory.
    ///
    /// @c ContentBrowser renders a two-column table: the left column shows
    /// sub-directories and the right column shows files. Each entry is drawn as a
    /// centred icon with a truncated label underneath. The user can:
    /// - Double-click a directory to descend into it.
    /// - Click the back button to ascend to the parent directory.
    /// - Drag a file entry onto a drop target (e.g. the material slot of an entity).
    /// - Receive a dropped path via @c renderDropTarget().
    ///
    /// @par Root / current path
    /// The browser starts at @p defaultPath (passed to the constructor) and never
    /// ascends above it. @c m_rootPath is fixed at construction; @c m_currentPath
    /// changes as the user navigates.
    ///
    /// @par Ownership
    /// @c ContentBrowser holds no heap resources beyond the @c std::vector caches
    /// that are refreshed on directory change.
    class ContentBrowser
    {
    public:
        /// @brief Constructs the browser rooted at @p defaultPath.
        ///
        /// Sets both @c m_rootPath and @c m_currentPath to @p defaultPath, then
        /// calls @c refreshFolder() to populate @c directories and @c files.
        ///
        /// @param defaultPath  Absolute path to the project's content directory.
        ///                     Must exist and be readable.
        explicit ContentBrowser(const std::filesystem::path& defaultPath);

        /// @brief Renders the complete content browser panel for the current frame.
        ///
        /// Calls @c renderBackButton(), then @c folderTable(). Must be called from
        /// within an active Dear ImGui window.
        void render();

    private:
        /// @brief Renders the two-column icon grid for the current directory.
        ///
        /// Iterates @c directories first, then @c files. Each cell calls the
        /// appropriate @c renderDirectoryEntry() or @c renderFileEntry() helper.
        void folderTable();

        /// @brief Scans @c m_currentPath and rebuilds @c directories and @c files.
        ///
        /// Clears both vectors, then iterates the directory with
        /// @c std::filesystem::directory_iterator, sorting entries: directories
        /// first, then files, both in lexicographic order.
        void refreshFolder();

        /// @brief Renders a single directory entry cell (icon + label + navigation).
        ///
        /// Double-clicking the cell descends into @p entry by setting
        /// @c m_currentPath and calling @c refreshFolder(). Also calls
        /// @c renderDropTarget() to allow files to be moved into the directory.
        ///
        /// @param entry  Absolute path of the directory to display.
        void renderDirectoryEntry(const std::filesystem::path& entry);

        /// @brief Renders a single file entry cell (icon + label + drag source).
        ///
        /// Selects the icon via @c gcep::getFileIcon(). Clicking the cell sets
        /// @c m_selectedPath. Calls @c renderDragSource() so the file can be
        /// dragged onto compatible drop targets.
        ///
        /// @param entry  Absolute path of the file to display.
        void renderFileEntry(const std::filesystem::path& entry);

        /// @brief Renders a horizontally centred text icon inside the current cell.
        ///
        /// @param icon  UTF-8 encoded icon glyph (typically a Font Awesome codepoint).
        void renderCenteredIcon(const std::string& icon);

        /// @brief Renders a horizontally centred, width-clipped label inside the current cell.
        ///
        /// @param label      Text to display (typically the file / directory stem).
        /// @param cellWidth  Available horizontal space in pixels. Defaults to 64.
        void renderCenteredLabel(const std::string& label, float cellWidth = 64.0f);

        /// @brief Begins a Dear ImGui drag-and-drop source for @p entry.
        ///
        /// Sets the drag payload type to @c "CONTENT_BROWSER_ITEM" and stores the
        /// UTF-8 path string as the payload data.
        ///
        /// @param entry  Absolute path to expose as the drag payload.
        void renderDragSource(const std::filesystem::path& entry);

        /// @brief Begins a Dear ImGui drop target for the given destination directory.
        ///
        /// Accepts payloads of type @c "CONTENT_BROWSER_ITEM". On successful drop,
        /// moves the dragged file into @p destination using @c std::filesystem::rename()
        /// and refreshes the folder.
        ///
        /// @param destination  Directory into which the dragged file should be moved.
        /// @returns            @c true if a drop was accepted and processed this frame.
        bool renderDropTarget(const std::filesystem::path& destination);

        /// @brief Renders a ".." back button and ascends one directory level when clicked.
        ///
        /// The button is disabled when @c m_currentPath == @c m_rootPath to prevent
        /// the user from navigating above the project root.
        void renderBackButton();

    private:
        /// @brief Number of columns used in the folder table (derived from panel width).
        int m_folderSize;

        /// @brief Topmost directory the browser is allowed to display. Never changes after construction.
        std::filesystem::path m_rootPath;

        /// @brief Directory currently displayed. Changes when the user navigates.
        std::filesystem::path m_currentPath;

        /// @brief Absolute path of the entry last clicked by the user. Empty if none.
        std::filesystem::path m_selectedPath;

        /// @brief Sorted list of sub-directories in @c m_currentPath. Refreshed on navigation.
        std::vector<std::filesystem::path> m_directories;

        /// @brief Sorted list of files in @c m_currentPath. Refreshed on navigation.
        std::vector<std::filesystem::path> m_files;
    };
} // namespace gcep::editor
