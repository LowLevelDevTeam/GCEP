#pragma once

/// @file scene.hpp
/// @brief Core scene type: owns the ECS registry, scene settings, and snapshot I/O.

// Internals
#include <ECS/headers/registry.hpp>
#include <ECS/Components/components.hpp>
#include <Maths/vector3.hpp>
#include <Maths/vector4.hpp>
#include <Scene/header/binary_scene_snapshot.hpp>
#include <Scene/header/json_scene_snapshot.hpp>

// STL
#include <string>
#include <vector>

// Forward declarations
namespace gcep::rhi::vulkan
{
    class VulkanRHI;
}

namespace gcep::SLS
{
    /// @brief Global rendering and lighting parameters stored per scene.
    ///
    /// Applied by the renderer at the start of each frame via @c Scene::onRender().
    /// All color channels are in linear space.
    struct SceneSettings
    {
        Vector4<float> clearColor     = { 0.1f, 0.1f, 0.1f, 1.0f }; ///< Framebuffer clear color (RGBA, linear).
        Vector3<float> ambientColor   = { 0.2f, 0.2f, 0.2f };       ///< Hemisphere ambient light color (RGB, linear).
        Vector3<float> lightColor     = { 0.5f, 0.5f, 0.5f };       ///< Directional light color (RGB, linear).
        Vector3<float> lightDirection = { 1.0f, 1.0f, 0.0f };       ///< World-space directional light direction (need not be normalised).
    };

    /// @brief Owns an ECS registry, scene metadata, and serialisation helpers for one scene.
    ///
    /// A @c Scene is the top-level container for all entities and components in a
    /// level or game state. It drives two distinct update loops — @c onUpdateEditor()
    /// for editor-mode ticking (no simulation) and @c onUpdateRuntime() for
    /// play-mode ticking — and exposes @c onRender() for the rendering pass.
    ///
    /// Persistence is handled by @c JsonSnapShot and @c BinarySnapShot; call
    /// @c load() / @c save() rather than using the snapshot objects directly.
    ///
    /// @par Typical lifecycle
    /// @code
    /// Scene scene("Level_01");
    /// scene.load("assets/scenes/level_01.bin", rhi);
    ///
    /// while (running)
    /// {
    ///     scene.onUpdateRuntime(dt);
    ///     scene.onRender();
    /// }
    ///
    /// scene.save();
    /// @endcode
    class Scene
    {
    public:
        /// @brief Constructs an empty scene with the given display name.
        /// @param name  Human-readable name shown in the editor. Defaults to @c "New Scene".
        explicit Scene(const std::string& name = "New Scene");

        ~Scene();

        // ── Update / render ───────────────────────────────────────────────────

        /// @brief Ticks all editor-mode systems for one frame.
        ///
        /// Called when the simulation is **not** running. Drives transform
        /// propagation, gizmo updates, and other editor-only logic.
        ///
        /// @param deltaTime  Elapsed time since the previous frame (seconds).
        void onUpdateEditor(float deltaTime);

        /// @brief Ticks all runtime systems for one frame.
        ///
        /// Called when the simulation is running. Drives physics, scripting,
        /// animation, and all gameplay systems.
        ///
        /// @param deltaTime  Elapsed time since the previous frame (seconds).
        void onUpdateRuntime(float deltaTime);

        /// @brief Submits the scene's renderable entities to the RHI for this frame.
        ///
        /// Should be called once per frame after @c onUpdateEditor() or
        /// @c onUpdateRuntime().
        void onRender();

        // ── Entity management ─────────────────────────────────────────────────

        /// @brief Creates a new entity with a @c NameComponent and default components.
        ///
        /// @param name  Display name assigned to the entity's @c NameComponent.
        /// @return      The new entity's ID, stable for the lifetime of this scene.
        ECS::EntityID createEntity(const std::string& name);

        /// @brief Destroys the entity and all its components, and removes it from the hierarchy.
        ///
        /// @param id  Entity to destroy. Must be a valid entity in this scene's registry.
        void destroyEntity(ECS::EntityID id);

        /// @brief Returns the direct children of @p parent in scene-graph order.
        ///
        /// @param parent  Entity whose children are queried.
        /// @return        List of direct child entity IDs; empty if @p parent is childless.
        std::vector<ECS::EntityID> getChildren(ECS::EntityID parent);

        /// @brief Looks up an entity by display name.
        ///
        /// Performs a linear scan of all @c NameComponent entries.
        ///
        /// @param name  The name to search for.
        /// @return      The matching entity ID, or @c GCEP_INVALID_ENTITY if not found.
        ECS::EntityID getEntity(const std::string& name);

        // ── Hierarchy ─────────────────────────────────────────────────────────

        /// @brief Reparents @p child under @p parent in the scene hierarchy.
        ///
        /// Updates @c HierarchyComponent linkage on both entities. If @p child
        /// already has a parent it is detached first.
        ///
        /// @param child   Entity to reparent.
        /// @param parent  New parent entity.
        void setParent(ECS::EntityID child, ECS::EntityID parent);

        /// @brief Detaches @p child from its current parent, making it a scene root.
        ///
        /// No-op if @p child has no parent.
        ///
        /// @param child  Entity to detach.
        void removeParent(ECS::EntityID child);

        // ── Persistence ───────────────────────────────────────────────────────

        /// @brief Clears the current scene state and loads from the file at @p filename.
        ///
        /// Selects binary or JSON deserialisation based on the file extension.
        /// Passes @p rhi so that GPU resources (meshes, textures) can be uploaded
        /// immediately during load.
        ///
        /// @param filename  Path to the scene file on disk.
        /// @param rhi       Vulkan RHI used to upload GPU resources during load.
        void load(const std::string& filename, rhi::vulkan::VulkanRHI* rhi);

        /// @brief Saves the current scene state to @c m_path.
        ///
        /// The serialisation format (binary or JSON) is determined by the
        /// extension of @c m_path. @c m_path must be set via @c setPath() before
        /// the first call.
        void save();

        /// @brief Destroys all entities and resets the registry to an empty state.
        ///
        /// Does not reset @c m_name, @c m_path, or @c m_settings.
        void clear();

        // ── Accessors ─────────────────────────────────────────────────────────

        /// @brief Returns a mutable reference to the scene's ECS registry.
        ECS::Registry&       getRegistry()       { return m_registry; }
        /// @brief Returns an immutable reference to the scene's ECS registry.
        const ECS::Registry& getRegistry() const { return m_registry; }

        /// @brief Returns the scene's display name.
        const std::string& getName() const { return m_name; }

        /// @brief Returns the file-system path associated with this scene.
        ///
        /// Used by @c save() as the output destination. Empty until @c setPath()
        /// or @c load() is called.
        const std::string& getPath() const { return m_path; }

        /// @brief Sets the file-system path used by subsequent @c save() calls.
        /// @param path  Absolute or project-relative file path.
        void setPath(const std::string& path) { m_path = path; }

        /// @brief Sets the scene's display name.
        /// @param name  New name string.
        void setName(const std::string& name) { m_name = name; }

        /// @brief Replaces the scene's rendering and lighting settings.
        /// @param settings  New @c SceneSettings to apply.
        void setSceneSettings(const SceneSettings& settings) { m_settings = settings; }

        /// @brief Returns a mutable reference to the scene's rendering and lighting settings.
        SceneSettings& getSceneSettings() { return m_settings; }

    private:
        std::string   m_name;     ///< Display name shown in the editor and stored in save files.
        std::string   m_path;     ///< File-system path used by @c save(); empty until assigned.
        ECS::Registry m_registry; ///< Owns all entities and components belonging to this scene.
        SceneSettings m_settings; ///< Global rendering and lighting parameters for this scene.
    };

} // namespace gcep::SLS

#include <Scene/detail/scene.inl>
#include <Scene/detail/json_scene_snapshot.inl>
#include <Scene/detail/binary_scene_snapshot.inl>
