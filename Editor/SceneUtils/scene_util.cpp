#include <Editor/SceneUtils/scene_util.hpp>
#include <filesystem>
#include <Externals/glm/glm/gtc/constants.hpp>
#include <Externals/glm/glm/gtc/quaternion.hpp>

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

ECS::EntityID spawnAsset(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi,
                         const std::string& path, glm::vec3 pos)
{
    const ECS::EntityID id = scene.createEntity(
        std::filesystem::path(path).stem().string()
    );

    auto& pc      = scene.getRegistry().addComponent<ECS::PhysicsComponent>(id);
    pc.motionType = ECS::EMotionType::STATIC;
    pc.layers     = ECS::ELayers::NON_MOVING;
    pc.shapeType  = ECS::EShapeType::CUBE;

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
    auto& pc = scene.getRegistry().getComponent<ECS::PhysicsComponent>(id);
    pc.shapeType    = ECS::EShapeType::CYLINDER;
    auto& tc        = scene.getRegistry().getComponent<ECS::Transform>(id);
    tc.eulerRadians = { glm::pi<float>() / 2.0f, 0.0f, 0.0f };
    glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
    tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
    return id;
}

ECS::EntityID spawnIcosphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
{
    auto  id = spawnAsset(scene, rhi, "Assets/Models/icosphere.obj", pos);
    auto& pc = scene.getRegistry().getComponent<ECS::PhysicsComponent>(id);
    pc.shapeType = ECS::EShapeType::SPHERE;
    return id;
}

ECS::EntityID spawnSphere(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
{
    auto  id = spawnAsset(scene, rhi, "Assets/Models/sphere.obj", pos);
    auto& pc = scene.getRegistry().getComponent<ECS::PhysicsComponent>(id);
    pc.shapeType = ECS::EShapeType::SPHERE;
    return id;
}

ECS::EntityID spawnSuzanne(SLS::Scene& scene, rhi::vulkan::VulkanRHI* rhi, glm::vec3 pos)
{
    auto  id = spawnAsset(scene, rhi, "Assets/Models/suzanne.obj", pos);
    auto& tc = scene.getRegistry().getComponent<ECS::Transform>(id);
    tc.eulerRadians = { glm::pi<float>() / 2.0f, 0.0f, glm::pi<float>() / 2.0f };
    glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
    tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
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
