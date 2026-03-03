#pragma once
#include <string>
#include <vector>
#include <Engine/Core/ECS/headers/registry.hpp>
#include <Engine/RHI/RHI.hpp>


namespace  gcep::SLS
{
    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        void onUpdateEditor(float deltaTime);
        void onUpdateRuntime(float deltaTime);
        void onRender();
        ECS::EntityID createEntity(const std::string& name);
        void destroyEntity(ECS::EntityID id);

        inline ECS::Registry& getRegistry() { return m_registry; }
        inline const std::string& getName() const { return m_name; }
        void setName(const std::string& name) { m_name = name; }

    private:
        std::string m_name;
        ECS::Registry m_registry;
    };

}

#include  <Engine/Core/Scene/detail/scene.inl>