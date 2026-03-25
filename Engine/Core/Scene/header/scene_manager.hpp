#pragma once

/// @file scene_manager.hpp
/// @brief Singleton that owns the active scene and manages the scene registry.

// Internals
#include <Scene/header/scene.hpp>

// STL
#include <memory>
#include <string>
#include <vector>

namespace gcep::SLS
{
    /// @brief Singleton gateway to the active @c Scene and the project's scene list.
    ///
    /// @c SceneManager owns exactly one @c Scene at a time (@c m_currentScene) and
    /// maintains a flat list of known scene file paths. It is the authoritative
    /// entry point for loading, saving, and ticking the active scene.
    ///
    /// @par Typical usage
    /// @code
    /// auto& sm = SceneManager::instance();
    /// sm.registerScene("assets/scenes/level_01.json");
    /// sm.loadScene   ("assets/scenes/level_01.json", rhi);
    ///
    /// while (running)
    ///     sm.update(dt);
    ///
    /// sm.saveScene();
    /// @endcode
    ///
    /// @note The class is non-copyable and non-movable. Obtain the singleton via
    ///       @c instance() — do not construct it directly.
    class SceneManager
    {
    public:
        /// @brief Returns the process-wide singleton instance.
        static SceneManager& instance();

        // ── Active scene ──────────────────────────────────────────────────────

        /// @brief Returns a reference to the currently loaded scene.
        ///
        /// @pre A scene must have been loaded via @c loadScene() before calling
        ///      this method; behaviour is undefined if @c m_currentScene is null.
        Scene& current();

        /// @brief Saves the current scene to its associated file path.
        ///
        /// Delegates to @c Scene::save(). The scene's path must have been set
        /// (either by @c loadScene() or by @c Scene::setPath()) before calling
        /// this method.
        void saveScene();

        /// @brief Replaces the active scene by loading from @p path.
        ///
        /// Destroys the current scene (if any), constructs a new @c Scene, and
        /// calls @c Scene::load(). The new scene becomes the result of subsequent
        /// @c current() calls.
        ///
        /// @param path  File-system path to the scene file (binary or JSON).
        /// @param rhi   Vulkan RHI used to upload GPU resources during load.
        void loadScene(const std::string& path, rhi::vulkan::VulkanRHI* rhi);

        // ── Scene registry ────────────────────────────────────────────────────

        /// @brief Adds @p path to the project's known scene list.
        ///
        /// The scene list is used by the editor's scene-picker UI and does not
        /// trigger a load. Duplicate paths are not deduplicated automatically.
        ///
        /// @param path  File-system path to the scene asset.
        void registerScene(const std::string& path);

        /// @brief Returns the list of all registered scene file paths.
        [[nodiscard]] const std::vector<std::string>& getSceneList() const;

        // ── Per-frame tick ────────────────────────────────────────────────────

        /// @brief Forwards a frame tick to the active scene.
        ///
        /// Calls @c Scene::onUpdateEditor() or @c Scene::onUpdateRuntime()
        /// depending on the current simulation state, then @c Scene::onRender().
        ///
        /// @param deltaTime  Elapsed time since the previous frame (seconds).
        void update(float deltaTime);

    private:
        SceneManager() = default;
        SceneManager(const SceneManager&)            = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        std::unique_ptr<Scene>   m_currentScene; ///< Sole owner of the currently active scene; null until @c loadScene() is called.
        std::vector<std::string> m_sceneList;    ///< Ordered list of scene file paths registered with @c registerScene().
    };

} // namespace gcep::SLS

#include <Scene/detail/scene_manager.inl>
