#pragma once

/// @file project_loader.hpp
/// @brief Singleton that manages project creation, loading, saving, and settings.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Externals
#include <GLFW/glfw3.h>
#include <Externals/rapidjson/document.h>
#include <Externals/rapidjson/istreamwrapper.h>
#include <Externals/rapidjson/prettywriter.h>
#include <Externals/rapidjson/stringbuffer.h>

// STL
#include <filesystem>
#include <string>
#include <vector>

namespace gcep::pl
{
    /// @struct ProjectSettings
    /// @brief Serialisable, user-facing settings for a GCEP project.
    ///
    /// Every field maps 1-to-1 to a JSON key written by @c project_loader::writeProjectFile().
    /// Default values are chosen to give a useful out-of-the-box editing experience.
    struct ProjectSettings
    {
        /// @brief Enable vertical synchronisation for the swap chain. Default: false.
        bool vsync = false;

        /// @brief RGBA clear colour for the main framebuffer. Default: dark grey.
        float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

        /// @brief Ambient light colour contribution. Default: low-intensity white.
        float ambientColor[3] = { 0.2f, 0.2f, 0.2f };

        /// @brief Directional light colour. Default: full-intensity white.
        float lightColor[3] = { 1.0f, 1.0f, 1.0f };

        /// @brief Directional light direction vector (world space). Default: straight down.
        float lightDirection[3] = { 0.0f, -1.0f, 0.0f };

        /// @brief Editor camera movement speed (world units / second). Default: 5.
        float cameraSpeed = 5.0f;

        /// @brief Size of a single grid cell in world units. Default: 1.
        float gridCellSize = 1.0f;

        /// @brief Draw a thicker grid line every N cells. Default: 5.
        float gridThickEvery = 5.0f;

        /// @brief Distance at which the infinite grid begins to fade out. Default: 50.
        float gridFadeDistance = 50.0f;

        /// @brief Width of the grid lines in pixels. Default: 1.
        float gridLineWidth = 1.0f;

        /// @brief Temporal anti-aliasing blend factor (0 = no TAA, 1 = full history). Default: 0.10.
        float taaBlendAlpha = 0.10f;
    };

    /// @struct ProjectInfo
    /// @brief Aggregates all metadata and settings that describe a GCEP project on disk.
    struct ProjectInfo
    {
        /// @brief Absolute path to the project's root directory.
        std::filesystem::path projectPath;

        /// @brief Absolute path to the project's content (asset) directory.
        std::filesystem::path contentPath;

        /// @brief Relative path (from @c projectPath) to the scene loaded at startup.
        std::filesystem::path startScene;

        /// @brief Human-readable project name displayed in the title bar.
        std::string projectName;

        /// @brief Semantic version string. Default: @c "1.0".
        std::string version = "1.0";

        /// @brief ISO 8601 creation timestamp set when the project file is first written.
        std::string createdAt;

        /// @brief Runtime-tweakable rendering and editor settings.
        ProjectSettings settings;
    };

    /// @class ProjectLoader
    /// @brief Singleton responsible for project lifecycle management.
    ///
    /// @c ProjectLoader is the single point of truth for the active project. It:
    /// - Presents an ImGui project-selection / creation UI at startup.
    /// - Reads and writes the @c .gcepproj JSON file via RapidJSON.
    /// - Exposes runtime settings through @c getSettings() so other systems can
    ///   read them without knowing the file format.
    /// - Auto-saves the project every @c m_autoSaveInterval seconds and debounces
    ///   dirty writes via @c m_dirtyDebounce.
    ///
    /// @par Singleton access
    /// @code
    /// pl::ProjectLoader& loader = pl::ProjectLoader::instance();
    /// loader.init(glfwWindow);
    /// @endcode
    ///
    /// @note The copy constructor and copy-assignment operator are deleted to enforce
    ///       the singleton invariant.
    class ProjectLoader
    {
    public:
        /// @brief Returns the process-wide singleton instance.
        /// @returns Reference to the unique @c ProjectLoader instance.
        static ProjectLoader& instance();

        /// @brief Binds the loader to the application window and resets internal state.
        ///
        /// Must be called once before any other method. Stores @p window so that
        /// file dialogs can be parented correctly.
        ///
        /// @param window  The application's main GLFW window. Must outlive this object.
        void init(GLFWwindow* window);

        /// @brief Advances auto-save and dirty-debounce timers.
        ///
        /// Should be called once per frame. When @c m_dirty is true and at least
        /// @c m_dirtyDebounce seconds have elapsed since the last modification,
        /// calls @c saveProject(). Also triggers @c saveProject() every
        /// @c m_autoSaveInterval seconds regardless of the dirty flag.
        ///
        /// @param deltaTime  Elapsed time since the previous frame, in seconds.
        void update(double deltaTime);

        /// @brief Serialises the current @c ProjectInfo to disk.
        ///
        /// Calls @c writeProjectFile(). Resets @c m_dirty to @c false and
        /// restarts @c m_autoSaveTimer.
        void saveProject();

        /// @brief Loads a project from @p projFile and makes it the active project.
        ///
        /// Call this from the project browser panel to switch to another project.
        /// Triggers a full engine reload when combined with @c EditorContext::reloadRequested.
        ///
        /// @param projFile  Absolute path to a @c .gcepproj file.
        void openProject(const std::filesystem::path& projFile) { loadProject(projFile); }

        /// @brief Marks the project as modified, starting the debounce timer.
        ///
        /// The next @c update() that clears the debounce window will trigger an
        /// automatic save.
        void markDirty() { m_dirty = true; }

        /// @brief Renders the project-selection / creation ImGui window.
        ///
        /// Presents a list of recently opened projects and a "New project" dialog.
        /// Sets @p stillSelecting to @c false once the user has confirmed a selection
        /// or created a new project.
        ///
        /// @param[out] stillSelecting  Set to @c false when the user has finished selecting.
        void drawUI(bool& stillSelecting);

        /// @brief Returns a mutable reference to the project's runtime settings.
        ///
        /// Other systems (e.g. the renderer) call this to read settings each frame.
        ///
        /// @returns Reference to @c m_info.settings.
        [[nodiscard]] ProjectSettings& getSettings() { return m_info.settings; }

        /// @brief Returns a copy of the current project metadata.
        /// @returns A value copy of @c m_info.
        [[nodiscard]] ProjectInfo getProjectInfo() const { return m_info; }

    private:
        void refreshBrowserUI();
        void drawBrowserUI(bool& stillSelecting);





        ProjectLoader(const ProjectLoader&)            = delete;
        ProjectLoader& operator=(const ProjectLoader&) = delete;

    private:
        ProjectLoader() = default;

        /// @brief Parses @p projFile and populates @c m_info.
        ///
        /// Uses RapidJSON to deserialise the JSON project file. If any expected key
        /// is missing, the corresponding field retains its default value.
        ///
        /// @param projFile  Absolute path to a @c .gcepproj file.
        void loadProject(const std::filesystem::path& projFile);

        /// @brief Serialises @c m_info to the @c .gcepproj file on disk.
        ///
        /// Uses @c rapidjson::PrettyWriter to produce human-readable JSON. Creates
        /// the directory hierarchy if necessary.
        void writeProjectFile();

        /// @brief Creates a minimal default scene file in the project's content directory.
        ///
        /// Called the first time a new project is written so the editor always has
        /// a valid scene to open.
        void writeDefaultSceneFile();

        /// @brief Returns the current UTC time formatted as an ISO 8601 string.
        ///
        /// Example output: @c "2026-02-17T14:32:00Z"
        ///
        /// @returns ISO 8601 timestamp string.
        [[nodiscard]] std::string nowISO8601();

    private:
        /// @brief Non-owning pointer to the application window used for dialog parenting.
        GLFWwindow* m_window = nullptr;

        // File browser state (used in drawUI)
        std::filesystem::path              m_browserPath;
        std::vector<std::filesystem::path> m_browserFiles;
        std::filesystem::path              m_browserSelected;
        bool                               m_browserInitialized = false;

        /// @brief All metadata and settings for the currently open project.
        ProjectInfo m_info;

        /// @brief Temporary buffer for the project name text input widget.
        char m_projectName[256] = {};

        /// @brief Directory in which the new project will be created.
        std::filesystem::path m_createPath;

        /// @brief True when the project has unsaved changes.
        bool m_dirty = false;

        /// @brief Accumulated time since @c m_dirty was last set to @c true, in seconds.
        double m_dirtyTimer = 0.0;

        /// @brief Minimum number of seconds between a @c markDirty() call and the resulting save.
        double m_dirtyDebounce = 2.0;

        /// @brief Accumulated time since the last auto-save, in seconds.
        double m_autoSaveTimer = 0.0;

        /// @brief Number of seconds between automatic saves. Default: 300 (5 minutes).
        double m_autoSaveInterval = 300.0;
    };
} // namespace gcep::pl
