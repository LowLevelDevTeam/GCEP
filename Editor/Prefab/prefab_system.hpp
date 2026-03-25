#pragma once

/// @file prefab_system.hpp
/// @brief Save and instantiate entity prefabs (.gcprefab).

// Internals
#include <ECS/headers/registry.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi.hpp>
#include <Scene/header/scene.hpp>

// Externals
#include <glm/vec3.hpp>

// STL
#include <filesystem>
#include <string>

namespace gcep::editor
{
    /// @brief Prefab magic string written in every .gcprefab file.
    inline constexpr const char* PREFAB_MAGIC = "GCEP_PREFAB";

    class PrefabSystem
    {
    public:
        /// @brief Serializes the selected entity's ECS components (and mesh path if any)
        ///        to a .gcprefab JSON file.
        ///
        /// HierarchyComponent and WorldTransform are intentionally excluded
        /// (they are runtime/computed data).
        static void save(ECS::Registry&          registry,
                         ECS::EntityID           id,
                         const std::string&      meshPath,
                         const std::filesystem::path& outPath);

        /// @brief Creates a new entity and restores its components from a .gcprefab file.
        ///
        /// @param pos  World-space spawn position (overrides saved Transform.position).
        /// @returns    The newly created entity ID, or ECS::INVALID_VALUE on failure.
        static ECS::EntityID instantiate(SLS::Scene&              scene,
                                         rhi::vulkan::VulkanRHI*  rhi,
                                         const std::filesystem::path& path,
                                         glm::vec3                pos);
    };

} // namespace gcep::editor
