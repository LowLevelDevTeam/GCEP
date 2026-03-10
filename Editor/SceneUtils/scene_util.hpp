#pragma once
#include <Engine/Core/Scene/header/scene.hpp>
#include <Engine/RHI/Vulkan/VulkanRHI.hpp>
#include <Externals/glm/glm/vec3.hpp>
#include <Engine/Core/ECS/Components/components.hpp>

#include "Engine/RHI/Mesh/Mesh.hpp"

namespace gcep::editor
{
    ECS::EntityID spawnAsset   (SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, const std::string& path, glm::vec3 pos);
    ECS::EntityID spawnCube    (SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
    ECS::EntityID spawnCone    (SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
    ECS::EntityID spawnCylinder(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
    ECS::EntityID spawnIcosphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
    ECS::EntityID spawnSphere  (SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
    ECS::EntityID spawnSuzanne (SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
    ECS::EntityID spawnTorus   (SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos);
}
