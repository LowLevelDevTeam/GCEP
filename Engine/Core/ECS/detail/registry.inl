#pragma once
#include <Engine/Core/ECS/headers/registry.hpp>

namespace gcep::ECS
{
    inline EntityID Registry::createEntity()
    {
        return m_idGenerator.generateID();
    }

    inline void Registry::destroyEntity(EntityID entity)
    {
        if (m_idGenerator.isValid(entity))
        {
            m_entitiesToDestroy.push_back(entity);
        }
    }

    inline void Registry::update() {
        for (auto toRemove : m_entitiesToDestroy)
        {
            if (m_idGenerator.isValid(toRemove))
            {
                removeEntity(toRemove);
            }
        }
        m_entitiesToDestroy.clear();
    }

    template<typename T, typename... Args>
    T& Registry::addComponent(EntityID entityID, Args&&... args)
    {
        return getPool<T>().add(entityID, std::forward<Args>(args)...);
    }

    template<typename T>
    void Registry::removeComponent(EntityID entityID)
    {
        getPool<T>().removeComponent(entityID);
    }

    template<typename T>
    T &Registry::getComponent(EntityID entity_id)
    {
        return getPool<T>().get(entity_id);
    }

    template<typename T>
    bool Registry::hasComponent(EntityID entity)  {
        return getPool<T>().hasComponent(entity);
    }

    template<typename... Args>
    View<Args...> Registry::view()
    {
        return View<Args...>(*this);
    }

    template<typename T>
    ComponentPool<T>& Registry::getPool()
    {
        uint32_t id = ComponentIDGenerator::get<T>();

        if (id >= m_pools.size())
        {
            m_pools.resize(id + 1);
        }
        if (!m_pools[id])
        {
            m_pools[id] = std::make_unique<ComponentPool<T>>();
        }

        return static_cast<ComponentPool<T>&>(*m_pools[id]);
    }

    inline void Registry::removeEntity(EntityID toRemove)
    {
        for (auto& pool : m_pools)
        {
            if (pool)
            {
                pool->removeComponent(toRemove);
            }
        }
        m_idGenerator.destroyEntity(toRemove);
    }

}
