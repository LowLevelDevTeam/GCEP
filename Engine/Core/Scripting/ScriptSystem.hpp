#pragma once

#include <Engine/Core/ECS/headers/registry.hpp>
#include <Engine/Core/RHI/Vulkan/VulkanMesh.hpp>

namespace gcep::scripting
{
    class ScriptSystem
    {
    public:
        void setMeshList(std::vector<rhi::vulkan::VulkanMesh>* meshes);
        void update(ECS::Registry& registry, double deltaSeconds);

    private:
        rhi::vulkan::VulkanMesh* findMesh(std::uint32_t meshId) const;

    private:
        std::vector<rhi::vulkan::VulkanMesh>* m_meshes = nullptr;
    };
}
