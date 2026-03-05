#pragma once
#include "PhysicsWrapper/Component/transform_component.hpp"
#include "Scene/header/scene.hpp"


namespace gcep::SLS
{

    Scene::Scene(const std::string& name) : m_name(name)
    {
        m_registry.getPool<TransformComponent>().reserve(1000);
        m_registry.getPool<TagComponent>().reserve(1000);
        m_registry.getPool<HierarchyComponent>().reserve(1000);
    }

    Scene::~Scene()
    {
        clear();
    }


    inline void Scene::onUpdateEditor(float deltaTime)
    {
        onRender();
        m_registry.update();

    }

    inline void Scene::onUpdateRuntime(float deltaTime)
    {
        onRender();
        m_registry.update();
    }

    inline ECS::EntityID Scene::createEntity(const std::string& name)
    {
        auto entity = m_registry.createEntity();
        m_registry.addComponent<TagComponent>(entity, name.empty() ? "Entity" : name);
        m_registry.addComponent<TransformComponent>(entity);
        m_registry.addComponent<WorldTransformComponent>(entity);
        m_registry.addComponent<HierarchyComponent>(entity);
        return entity;
    }


    inline void Scene::load(const std::string& path)
    {
        clear();
        JsonSnapShot snapshot(*this);
        snapshot.deserializeAll(path);
    }

    inline void Scene::save(const std::string& path)
    {
        SER::JsonSnapShot snapshot(*this);
        snapshot.serializeAll(path);
    }


    inline void Scene::destroyEntity(ECS::EntityID id)
    {
        for (auto child : getChildren(id))
        {
            destroyEntity(child);
        }

        removeParent(id);
        m_registry.destroyEntity(id);
    }

    inline std::vector<ECS::EntityID> Scene::getChildren(ECS::EntityID parent)
    {
        std::vector<ECS::EntityID> children;

        if (!m_registry.hasComponent<HierarchyComponent>(parent))
            return children;

        auto& parentHierarchy = m_registry.getComponent<HierarchyComponent>(parent);

        ECS::EntityID current = parentHierarchy.firstChild;
        while (current != ECS::INVALID_VALUE)
        {
            children.push_back(current);
            auto& h = m_registry.getComponent<HierarchyComponent>(current);
            current = h.nextSibling;
        }

        return children;
    }

    inline ECS::EntityID Scene::getEntity(const std::string& name)
    {
        auto view = m_registry.view<Tag>();
        for (auto entity : view)
        {
            if (view.get<Tag>(entity).name == name)
                return entity;
        }
        return ECS::INVALID_VALUE;
    }

    inline void Scene::setParent(ECS::EntityID child, ECS::EntityID parent)
    {
        if (!m_registry.hasComponent<HierarchyComponent>(child) ||
            !m_registry.hasComponent<HierarchyComponent>(parent))
            return;
        removeParent(child);

        auto& childHierarchy  = m_registry.getComponent<HierarchyComponent>(child);
        auto& parentHierarchy = m_registry.getComponent<HierarchyComponent>(parent);

        if (parentHierarchy.firstChild != ECS::INVALID_VALUE)
        {
            auto& firstChildHierarchy = m_registry.getComponent<HierarchyComponent>(parentHierarchy.firstChild);
            firstChildHierarchy.prevSibling = child;
            childHierarchy.nextSibling = parentHierarchy.firstChild;
        }

        parentHierarchy.firstChild = child;
    }

    inline void Scene::removeParent(ECS::EntityID child)
    {
        if (!m_registry.hasComponent<HierarchyComponent>(child)) return;

        auto& childHierarchy = m_registry.getComponent<HierarchyComponent>(child);
        if (childHierarchy.parent == ECS::INVALID_VALUE) return;

        auto& parentHierarchy = m_registry.getComponent<HierarchyComponent>(childHierarchy.parent);

        if (childHierarchy.prevSibling != ECS::INVALID_VALUE)
        {
            auto& prev = m_registry.getComponent<HierarchyComponent>(childHierarchy.prevSibling);
            prev.nextSibling = childHierarchy.nextSibling;
        }
        else
        {
            parentHierarchy.firstChild = childHierarchy.nextSibling;
        }

        if (childHierarchy.nextSibling != ECS::INVALID_VALUE)
        {
            auto& next = m_registry.getComponent<HierarchyComponent>(childHierarchy.nextSibling);
            next.prevSibling = childHierarchy.prevSibling;
        }

        childHierarchy.parent      = ECS::INVALID_VALUE;
        childHierarchy.prevSibling = ECS::INVALID_VALUE;
        childHierarchy.nextSibling = ECS::INVALID_VALUE;
    }


    inline void Scene::clear()
    {
        for (auto& pool : m_registry.getPools())
        {
            if (!pool) continue;
            auto entities = pool->getEntities();
            for (auto entity : entities)
            {
                m_registry.destroyEntity(entity);
            }
        }
        m_registry.update();

    }
}
