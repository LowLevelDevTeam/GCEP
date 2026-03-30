#pragma once

/// @file content_browser.hpp
/// @brief Dockable file-system browser panel for the editor.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Internals
#include <Editor/UI_Panels/ipanel.hpp>
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
    /// @c ContentBrowser renders a toolbar, an optional folder-tree side-panel, a
    /// breadcrumb trail, and either a grid or a list view of the current directory's
    /// contents.  Each entry is drawn as a centred icon with a truncated label.
    /// The user can:
    /// - Double-click a directory to descend into it.
    /// - Click the back button (or breadcrumb) to ascend.
    /// - Drag a file entry onto a drop target (e.g. a material slot in the inspector).
    /// - Receive a file dropped from the OS via @c processDroppedFiles().
    /// - Right-click an item or the background to open context menus for rename /
    ///   delete / create operations.
    ///
    /// @par Root / current path
    /// The browser starts at @p defaultPath (passed to the constructor) and never
    /// ascends above it. @c m_rootPath is fixed at construction time; @c m_currentPath
    /// changes as the user navigates.
    ///
    /// @par Ownership
    /// @c ContentBrowser holds no heap resources beyond the @c std::vector caches
    /// that are refreshed on every directory change.
    class ContentBrowser : public panel::IPanel
    {
    public:
        /// @brief Constructs the browser rooted at @p defaultPath.
        ///
        /// Sets both @c m_rootPath and @c m_currentPath to @p defaultPath, then
        /// calls @c refreshFolder() to populate @c m_directories and @c m_files.
        ///
        /// @param defaultPath  Absolute path to the project's content directory.
        ///                     Must exist and be readable.
        explicit ContentBrowser(const std::filesystem::path& defaultPath);

        /// @brief Renders the complete content browser panel for the current frame.
        ///
        /// Calls @c drawToolbar(), @c drawFolderTree(), @c drawBreadcrumb(), and then
        /// either @c folderTable() or @c folderList() depending on @c m_viewMode.
        /// Must be called from within an active Dear ImGui window.
        void draw() override;

    private:
        /// @brief Renders the two-column icon grid for the current directory.
        ///
        /// Iterates @c m_directories first, then @c m_files.  Each cell calls the
        /// appropriate @c renderDirectoryEntry() or @c renderFileEntry() helper.
        void folderTable();

        /// @brief Scans @c m_currentPath and rebuilds @c m_directories and @c m_files.
        ///
        /// Clears both vectors, then iterates the directory with
        /// @c std::filesystem::directory_iterator and sorts entries: directories
        /// first, then files, both in lexicographic order.
        void refreshFolder();

        /// @brief Renders a single directory entry cell (icon + label + navigation).
        ///
        /// Double-clicking the cell descends into @p entry by updating
        /// @c m_currentPath and calling @c refreshFolder(). Also calls
        /// @c renderDropTarget() so files can be moved into the directory via drag-and-drop.
        ///
        /// @param entry  Absolute path of the directory to display.
        void renderDirectoryEntry(const std::filesystem::path& entry);

        /// @brief Renders a single file entry cell (icon + label + drag source).
        ///
        /// Selects the icon via @c gcep::getFileIcon(). Clicking the cell updates
        /// @c m_selectedPath. Calls @c renderDragSource() so the file can be dragged
        /// onto compatible drop targets.
        ///
        /// @param entry  Absolute path of the file to display.
        void renderFileEntry(const std::filesystem::path& entry);

        /// @brief Renders a horizontally centred text icon inside the current cell.
        ///
        /// @param icon  UTF-8 encoded icon glyph (typically a Font Awesome codepoint).
        void renderCenteredIcon(const std::string& icon);

        /// @brief Renders a horizontally centred, width-clipped label inside the current cell.
        ///
        /// @param label      Text to display (typically the file or directory stem).
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

        /// @brief Renders the top toolbar (view-mode toggle, search, create buttons).
        void drawToolbar();

        /// @brief Renders the collapsible folder-tree side panel.
        ///
        /// Calls @c drawFolderTreeNode() recursively from @c m_rootPath to build
        /// the full directory hierarchy. Selecting a node navigates to it.
        void drawFolderTree();

        /// @brief Recursively renders a single node of the folder tree.
        ///
        /// @param path  Absolute path of the directory node to render.
        void drawFolderTreeNode(const std::filesystem::path& path);

        /// @brief Renders a breadcrumb navigation trail for @c m_currentPath.
        ///
        /// Each path segment is a clickable button that navigates directly to that
        /// ancestor directory. The root segment is always shown; segments are
        /// separated by a "/" divider.
        void drawBreadcrumb();

        /// @brief Renders the current directory contents as a flat list (non-grid view).
        ///
        /// Used when @c m_viewMode is @c ViewMode::List.
        void folderList();

        /// @brief Renders a right-click context menu for @p path.
        ///
        /// Offers actions: Rename, Delete, and (for directories) Create inside.
        ///
        /// @param path  Absolute path of the item that was right-clicked.
        void drawItemContextMenu(const std::filesystem::path& path);

        /// @brief Renders a right-click context menu for the panel background.
        ///
        /// Offers actions applicable to @c m_currentPath: Create Folder, Create File.
        void drawBackgroundContextMenu();

        /// @brief Renders the rename modal dialog and applies the rename on confirmation.
        ///
        /// The modal is opened by setting @c m_openRenameModal to @c true and
        /// @c m_renamePath to the target path. On confirmation, the file or directory
        /// is renamed on disk and @c refreshFolder() is called.
        void drawRenameModal();

        /// @brief Renders the create-file / create-folder modal dialog.
        ///
        /// The modal is opened by @c requestCreateFolder() or @c requestCreateFile().
        /// On confirmation, delegates to @c createFolder() or @c createFile().
        void drawCreateModal();

        /// @brief Consumes file paths dropped onto the GLFW window by the OS.
        ///
        /// Reads @c EditorContext::droppedPaths, copies matching files into
        /// @c m_currentPath, then clears the list and calls @c refreshFolder().
        void processDroppedFiles();

        /// @brief Schedules the create modal to open with folder-creation mode.
        ///
        /// @param parent  Directory in which the new folder should be created.
        void requestCreateFolder(const std::filesystem::path& parent);

        /// @brief Schedules the create modal to open with file-creation mode.
        ///
        /// @param parent     Directory in which the new file should be created.
        /// @param extension  File extension (e.g. @c ".mat"), without a leading dot
        ///                   separating it from the stem if the dot is already included.
        void requestCreateFile(const std::filesystem::path& parent, const std::string& extension);

        /// @brief Creates a subdirectory named @p name inside @p parent on disk.
        ///
        /// Calls @c std::filesystem::create_directory() and then @c refreshFolder().
        ///
        /// @param parent  Parent directory in which to create the new folder.
        /// @param name    Name of the new subdirectory (must not contain path separators).
        void createFolder(const std::filesystem::path& parent, const std::string& name);

        /// @brief Creates an empty file named @p name + @p extension inside @p parent on disk.
        ///
        /// Opens the file for writing to create it, then calls @c refreshFolder().
        ///
        /// @param parent     Parent directory in which to create the new file.
        /// @param name       Stem of the new file (without extension).
        /// @param extension  File extension including the leading dot (e.g. @c ".lua").
        void createFile(const std::filesystem::path& parent, const std::string& name, const std::string& extension);

        // ── View mode ─────────────────────────────────────────────────────────────

        /// @brief Controls whether items are displayed in a grid or a flat list.
        enum class ViewMode { Grid, List };

        /// @brief Current view mode; defaults to @c Grid.
        ViewMode m_viewMode  = ViewMode::Grid;

        /// @brief Icon cell size in pixels for grid view; adjustable via the toolbar.
        int      m_folderSize;

        // ── Navigation ────────────────────────────────────────────────────────────

        /// @brief Fixed root of the browser; navigation never ascends above this path.
        std::filesystem::path m_rootPath;

        /// @brief Currently displayed directory; changes as the user navigates.
        std::filesystem::path m_currentPath;

        /// @brief Absolute path of the item last clicked by the user.
        std::filesystem::path m_selectedPath;

        // ── Cached directory contents ─────────────────────────────────────────────

        /// @brief Subdirectories of @c m_currentPath, refreshed on each navigation.
        std::vector<std::filesystem::path> m_directories;

        /// @brief Files in @c m_currentPath, refreshed on each navigation.
        std::vector<std::filesystem::path> m_files;

        // ── Rename modal state ────────────────────────────────────────────────────

        /// @brief Path of the item currently being renamed.
        std::filesystem::path m_renamePath;

        /// @brief Text input buffer for the rename modal.
        char                  m_renameBuffer[256] = {};

        /// @brief When @c true, the rename modal will open on the next frame.
        bool                  m_openRenameModal   = false;

        // ── Create modal state ────────────────────────────────────────────────────

        /// @brief Directory in which the new item will be created.
        std::filesystem::path m_createParent;

        /// @brief Extension for the new file; empty string means folder creation.
        std::string           m_createExtension;

        /// @brief Text input buffer for the create modal.
        char                  m_createNameBuffer[256] = {};

        /// @brief When @c true, the create modal will open on the next frame.
        bool                  m_openCreateModal    = false;

        // ── Deferred delete ───────────────────────────────────────────────────────

        /// @brief Path of an item pending deletion at the end of the current frame.
        ///
        /// Deferred to avoid invalidating directory iterators during the draw loop.
        std::filesystem::path m_pendingDelete;
    };

} // namespace gcep::editor