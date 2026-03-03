#pragma once
#include <Engine/Core/Scene/header/scene.hpp>
#include <Engine/Core/Scene/header/json_converter.hpp>
#include "Maths/vector3.hpp"

namespace gcep::SLS
{

    inline void Scene::onUpdateEditor(float deltaTime)
    {
        onRender(deltaTime);

    }

    inline ECS::EntityID Scene::createEntity(const std::string &name)
    {
        auto entity = m_registry.createEntity();
        TagComponent tag =  m_registry.addComponent<TagComponent>(entity, name.empty() ? "Entity" : name);
        TransformComponent  transform = m_registry.addComponent<TransformComponent>(entity,
        glm::vec3(0.0f),
        glm::vec3(0.0f),
        glm::vec3(1.0f)
        );
        return entity;
    }

    inline void Scene::destroyEntity(ECS::EntityID id)
    {
        m_registry.destroyEntity(id);
    }
}
