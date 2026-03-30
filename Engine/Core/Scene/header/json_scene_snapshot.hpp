#pragma once

/// @file json_scene_snapshot.hpp
/// @brief JSON-based serialisation and deserialisation of a complete scene state.

// Internals
#include <ECS/headers/json_archive.hpp>
#include <ECS/headers/component_registry.hpp>

namespace gcep::SLS
{
    class Scene;

    /// @brief Reads and writes a @c Scene to and from a JSON file on disk.
    ///
    /// @c JsonSnapShot delegates the actual component serialisation to
    /// @c ECS::JsonArchive and the engine's @c ComponentRegistry, so all
    /// registered component types are handled automatically. The resulting file
    /// is human-readable and suitable for version control.
    ///
    /// @note The @c .inl implementation is included at the bottom of @c scene.hpp
    ///       so that the full @c Scene definition is visible to the template code.
    class JsonSnapShot
    {
    public:
        /// @brief Constructs a snapshot bound to @p scene.
        /// @param scene  The scene to serialise from or deserialise into.
        ///               Must outlive this object.
        explicit JsonSnapShot(Scene& scene) : m_scene(scene) {}

        /// @brief Serialises the entire scene state to a JSON file at @p path.
        ///
        /// Iterates over every entity in @c m_scene's registry and writes each
        /// component to a JSON object keyed by component type name. Creates or
        /// overwrites the file at @p path.
        ///
        /// @param path  Destination file path (e.g. @c "assets/scenes/level1.json").
        void serializeAll(const std::string& path);

        /// @brief Deserialises a previously saved JSON file into @p scene, replacing its current state.
        ///
        /// Clears the bound scene, then reconstructs all entities and components
        /// from the JSON file at @p path. Component types unknown to the registry
        /// are silently skipped.
        ///
        /// @param path  Source file path produced by a prior call to @c serializeAll().
        void deserializeAll(const std::string& path);

    private:
        Scene& m_scene; ///< Non-owning reference to the scene being serialised or deserialised.
    };

} // namespace gcep::SLS

// .inl include in scene.hpp
