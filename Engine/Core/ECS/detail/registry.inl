#pragma once

namespace gcep::ECS
{
    inline EntityID Registry::createEntity()
    {
        auto id = m_idGenerator.generateID();
        Log::info(std::format("Entity created : id = {}",id));
        return id;
    }

    inline void Registry::createEntityWithID(EntityID entity)
    {
        m_idGenerator.forceID(entity);
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

    template<ComponentConcept T, typename... Args>
T& Registry::addComponent(EntityID entityID, Args&&... args)
    {
        uint32_t id = ComponentIDGenerator::get<T>();

        if (std::ranges::find(m_componentTypeList, id) == m_componentTypeList.end())
        {
            m_componentTypeList.push_back(id);
        }

        return getPool<T>().add(entityID, std::forward<Args>(args)...);
    }

    template<ComponentConcept T>
    void Registry::removeComponent(EntityID entityID)
    {
        getPool<T>().removeComponent(entityID);
    }

    template<ComponentConcept T>
    T &Registry::getComponent(EntityID entity_id)
    {
        return getPool<T>().get(entity_id);
    }

    template<ComponentConcept T>
    bool Registry::hasComponent(EntityID entity)  {
        return getPool<T>().hasComponent(entity);
    }

    template<ComponentConcept... Args>
    View<Args...> Registry::view()
    {
        return View<Args...>(*this);
    }

    template<ComponentConcept T>
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

    inline const std::vector<std::unique_ptr<IPool>> & Registry::getPools() const
    {
        return m_pools;
    }

    inline const std::vector<uint32_t> & Registry::getTypeList() const
    {
        return m_componentTypeList;
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
