#include "scene_util.hpp"

// Externals
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

// STL
#include <filesystem>

namespace gcep::editor
{
    // ─────────────────────────────────────────────────────────────────────────────
    // Internal helpers
    // ─────────────────────────────────────────────────────────────────────────────

    static ECS::EntityID spawnLight(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi,
                                    rhi::vulkan::LightType type, glm::vec3 pos)
    {
        std::string name = (type == rhi::vulkan::LightType::Point) ? "Point light " : "Spot light ";
        name += std::to_string(std::time(nullptr));

        const ECS::EntityID id = scene.createEntity(name);
        rhi->spawnLight(type, id, pos);
        return id;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Public — asset
    // ─────────────────────────────────────────────────────────────────────────────

    void attachMesh(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi,
                    ECS::EntityID id, const std::string& path)
    {
        auto& registry = scene.getRegistry();

        if (!registry.hasComponent<ECS::BoxColliderComponent>(id))
            registry.addComponent<ECS::BoxColliderComponent>(id);

        if (registry.hasComponent<ECS::Transform>(id)) {
            auto& tc = registry.getComponent<ECS::Transform>(id);
            const glm::vec3 pos = { tc.position.x, tc.position.y, tc.position.z };
            rhi->spawnAsset(const_cast<char*>(path.c_str()), id, pos);
        }
    }

    ECS::EntityID spawnAsset(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi,
                             const std::string& path, glm::vec3 pos)
    {
        const ECS::EntityID id = scene.createEntity(
            std::filesystem::path(path).stem().string()
        );

        scene.getRegistry().addComponent<ECS::BoxColliderComponent>(id);

        rhi->spawnAsset(const_cast<char*>(path.c_str()), id, pos);

        return id;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Public — primitives
    // ─────────────────────────────────────────────────────────────────────────────

    ECS::EntityID spawnCube(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        return spawnAsset(scene, rhi, "Assets/Models/cube.obj", pos);
    }

    ECS::EntityID spawnCone(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        auto  id = spawnAsset(scene, rhi, "Assets/Models/cone.obj", pos);
        auto& tc = scene.getRegistry().getComponent<ECS::Transform>(id);
        tc.eulerRadians = { glm::pi<float>() / 2.0f, 0.0f, 0.0f };
        glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
        tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
        return id;
    }

    ECS::EntityID spawnCylinder(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        auto  id = spawnAsset(scene, rhi, "Assets/Models/cylinder.obj", pos);
        scene.getRegistry().removeComponent<ECS::BoxColliderComponent>(id);
        scene.getRegistry().addComponent<ECS::CylinderColliderComponent>(id);
        auto& tc        = scene.getRegistry().getComponent<ECS::Transform>(id);
        tc.eulerRadians = { glm::pi<float>() / 2.0f, 0.0f, 0.0f };
        glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
        tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
        return id;
    }

    ECS::EntityID spawnIcosphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        auto id = spawnAsset(scene, rhi, "Assets/Models/icosphere.obj", pos);
        scene.getRegistry().removeComponent<ECS::BoxColliderComponent>(id);
        scene.getRegistry().addComponent<ECS::SphereColliderComponent>(id);
        return id;
    }

    ECS::EntityID spawnSphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        auto id = spawnAsset(scene, rhi, "Assets/Models/sphere.obj", pos);
        scene.getRegistry().removeComponent<ECS::BoxColliderComponent>(id);
        scene.getRegistry().addComponent<ECS::SphereColliderComponent>(id);
        return id;
    }

    ECS::EntityID spawnSuzanne(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        auto  id = spawnAsset(scene, rhi, "Assets/Models/suzanne.obj", pos);
        if (scene.getRegistry().hasComponent<ECS::Transform>(id)) {
            auto& tc = scene.getRegistry().getComponent<ECS::Transform>(id);
            tc.eulerRadians = { glm::pi<float>() / 2.0f, 0.0f, glm::pi<float>() / 2.0f };
            glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
            tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
        }
        return id;
    }

    ECS::EntityID spawnTorus(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        return spawnAsset(scene, rhi, "Assets/Models/torus.obj", pos);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Public — lights
    // ─────────────────────────────────────────────────────────────────────────────

    ECS::EntityID spawnPointLight(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        return spawnLight(scene, rhi, rhi::vulkan::LightType::Point, pos);
    }

    ECS::EntityID spawnSpotLight(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
    {
        return spawnLight(scene, rhi, rhi::vulkan::LightType::Spot, pos);
    }
} // namespace gcep::editor
