#pragma once
#include <string>
#include <vector>
#include <Scene/header/components.hpp>
#include <ECS/headers/registry.hpp>
#include <Engine/RHI/RHI.hpp>
#include "Maths/vector3.hpp"

namespace  gcep::SLS
{
    class Scene
    {
    public:
        explicit Scene(const std::string& name = "New Scene");
        ~Scene();

        void onUpdateEditor(float deltaTime);
        void onUpdateRuntime(float deltaTime);
        void onRender();

        ECS::EntityID createEntity(const std::string& name),
        void destroyEntity(ECS::EntityID id);

        std::vector<ECS::EntityID> getChildren(ECS::EntityID parent);

        ECS::EntityID getEntity(const std::string &name);


        void setParent(ECS::EntityID child, ECS::EntityID parent);
        void removeParent(ECS::EntityID child);

        void load(const std::string& filename);
        void save(const std::string& filename);
        void clear();


        ECS::Registry& getRegistry()       { return m_registry; }
        const ECS::Registry& getRegistry() const { return m_registry; }
        const std::string&   getName()     const { return m_name;     }
        void setName(const std::string& name) { m_name = name; }
    private:


        std::string m_name;
        ECS::Registry m_registry;


    }

}

#include  <Engine/Core/Scene/detail/scene.inl>