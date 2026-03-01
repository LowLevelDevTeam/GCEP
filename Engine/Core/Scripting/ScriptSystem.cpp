#include "ScriptSystem.hpp"

#include "ScriptComponent.hpp"

namespace gcep::scripting
{
    void ScriptSystem::setMeshList(std::vector<rhi::vulkan::VulkanMesh>* meshes)
    {
        m_meshes = meshes;
    }

    void ScriptSystem::update(ECS::Registry& registry, double deltaSeconds)
    {
        for (auto entity : registry.view<ScriptComponent>())
        {
            auto& component = registry.getComponent<ScriptComponent>(entity);
            if (!component.host)
            {
                continue;
            }
            component.host->setEcsContext(&registry, entity);
            component.host->setMeshContext(findMesh(component.meshId));
            component.host->update(deltaSeconds);
        }
    }

    rhi::vulkan::VulkanMesh* ScriptSystem::findMesh(std::uint32_t meshId) const
    {
        if (!m_meshes)
        {
            return nullptr;
        }
        for (auto& mesh : *m_meshes)
        {
            if (mesh.id == meshId)
            {
                return &mesh;
            }
        }
        return nullptr;
    }
}
