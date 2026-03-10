#pragma once
#include <string>
#include <vector>
#include <ECS/headers/registry.hpp>
#include <Scene/header/json_scene_snapshot.hpp>
#include <Scene/header/binary_scene_snapshot.hpp>
#include <Engine/Core/Maths/vector3.hpp>
#include <Engine/Core/Maths/vector4.hpp>
#include <Engine/Core/ECS/Components/components.hpp>

namespace gcep::rhi::vulkan {
    class VulkanRHI;
}

namespace  gcep::SLS
{
    struct SceneSettings
    {
        mth::Vector4<float> clearColor     = { 0.1f, 0.1f, 0.1f, 1.0f };
        Vector3<float>      ambientColor   = { 0.2f, 0.2f, 0.2f };
        Vector3<float>      lightColor     = { 0.5f, 0.5f, 0.5f };
        Vector3<float>      lightDirection = { 1.0f, 1.0f, 0.0f };
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
        void save(const std::string& filename);
        void clear();


        ECS::Registry& getRegistry()       { return m_registry; }
        const ECS::Registry& getRegistry() const { return m_registry; }
        const std::string&   getName()     const { return m_name;     }
        void setName(const std::string& name) { m_name = name; }
        void setSceneSettings(const SceneSettings& settings) { m_settings = settings; }
        SceneSettings& getSceneSettings() { return m_settings; }
    private:


        std::string m_name;
        ECS::Registry m_registry;
        SceneSettings m_settings;

    };

}

#include  <Engine/Core/Scene/detail/scene.inl>
#include <Engine/Core/Scene/detail/json_scene_snapshot.inl>
#include <Engine/Core/Scene/detail/binary_scene_snapshot.inl>
