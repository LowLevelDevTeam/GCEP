#pragma once

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
    struct SceneSettings
    {
        Vector4<float> clearColor     = { 0.1f, 0.1f, 0.1f, 1.0f };
        Vector3<float> ambientColor   = { 0.2f, 0.2f, 0.2f };
        Vector3<float> lightColor     = { 0.5f, 0.5f, 0.5f };
        Vector3<float> lightDirection = { 1.0f, 1.0f, 0.0f };
    };

    class Scene
    {
    public:
        explicit Scene(const std::string& name = "New Scene");
        ~Scene();

        void onUpdateEditor(float deltaTime);
        void onUpdateRuntime(float deltaTime);
        void onRender();

        ECS::EntityID createEntity(const std::string& name);
        void destroyEntity(ECS::EntityID id);

        std::vector<ECS::EntityID> getChildren(ECS::EntityID parent);

        ECS::EntityID getEntity(const std::string &name);


        void setParent(ECS::EntityID child, ECS::EntityID parent);
        void removeParent(ECS::EntityID child);

        void load(const std::string& filename, rhi::vulkan::VulkanRHI* rhi);
        void save();
        void clear();


        ECS::Registry& getRegistry()       { return m_registry; }
        const ECS::Registry& getRegistry() const { return m_registry; }
        const std::string&   getName()     const { return m_name;     }
        const std::string&   getPath()     const { return m_path;     }
        void                 setPath(const std::string& path) { m_path = path; }
        void setName(const std::string& name) { m_name = name; }
        void setSceneSettings(const SceneSettings& settings) { m_settings = settings; }
        SceneSettings& getSceneSettings() { return m_settings; }
    private:
        std::string m_name;
        std::string m_path;
        ECS::Registry m_registry;
        SceneSettings m_settings;
    };
} // namespace gcep::SLS

#include <Scene/detail/scene.inl>
#include <Scene/detail/json_scene_snapshot.inl>
#include <Scene/detail/binary_scene_snapshot.inl>
