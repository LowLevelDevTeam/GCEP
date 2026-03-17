#pragma once

/// @file scene_util.hpp
/// @brief Helper functions for spawning primitive and asset entities into a scene.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Internals
#include <ECS/Components/components.hpp>
#include <Engine/RHI/Mesh/mesh.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi.hpp>
#include <Scene/header/scene.hpp>

// Externals
#include <glm/vec3.hpp>

namespace gcep::editor
{
    /// @brief Spawns an external OBJ asset into @p scene at position @p pos.
    ///
    /// Loads the mesh from @p path through the RHI, creates an ECS entity with
    /// a @c MeshComponent and a @c Transform initialised to @p pos, and registers
    /// the entity with the scene manager.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI used to upload the mesh data.
    /// @param path   Filesystem path to the OBJ (or supported mesh) file.
    /// @param pos    World-space position at which the asset is spawned.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnAsset(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi,
                             const std::string& path, glm::vec3 pos);

    /// @brief Attaches a mesh to an existing entity (does not create a new entity).
    void attachMesh(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi,
                    ECS::EntityID id, const std::string& path);

    /// @brief Spawns a unit cube primitive into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnCube(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns a cone primitive into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnCone(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns a cylinder primitive into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnCylinder(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns an icosphere primitive into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnIcosphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns a UV sphere primitive into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnSphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns the Suzanne (Blender monkey) mesh into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnSuzanne(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns a torus primitive into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created entity.
    ECS::EntityID spawnTorus(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns a point light into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created point light.
    ECS::EntityID spawnPointLight(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);

    /// @brief Spawns a spot light into @p scene at position @p pos.
    ///
    /// @param scene  Target scene that will own the new entity.
    /// @param rhi    Pointer to the active Vulkan RHI.
    /// @param pos    World-space spawn position.
    /// @returns      The ECS entity ID of the newly created spot light.
    ECS::EntityID spawnSpotLight(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
} // namespace gcep::editor
