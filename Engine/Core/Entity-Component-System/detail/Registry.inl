#pragma once
#include <Engine/Core/Entity-Component-System/headers/Registry.hpp>
namespace gcep
{
    inline Entity Registry::createEntity()
    {
        Entity newEntity;
        if (!freeIDs.empty())
        {
            newEntity = freeIDs.back();
            allEntities[newEntity] = newEntity;
            freeIDs.pop_back();
        }
        else
        {
            newEntity = static_cast<Entity>(allEntities.size());
            allEntities.push_back(newEntity);
        }
        return newEntity;
    }

    inline void Registry::removeEntity(Entity toRemove)
    {
        for (auto& pool : pools)
        {
            if (pool && pool->hasComponent(toRemove))
            {
                pool->removeComponent(toRemove);
            }
        }
        allEntities[toRemove] = INVALID_VALUE;
        freeIDs.push_back(toRemove);
    }

    template<typename T>
    ComponentPool<T>& Registry::getPool()
    {
        Signature id = ComponentIDGenerator::get<T>();

        if (id >= pools.size()) {
            pools.resize(id + 1);
        }
        if (!pools[id]) {
            pools[id] = std::make_unique<ComponentPool<T>>();
        }

        return static_cast<ComponentPool<T>&>(*pools[id]);
    }

    template<typename T>
    void Registry::addComponent(Entity entityID)
    {
        getPool<T>().addComponent(entityID);
    }

    template<typename T>
    void Registry::removeComponent(Entity entityID)
    {
        getPool<T>().removeComponent(entityID);
    }

    template<typename T>
    T &Registry::getComponent(Entity entity)
    {
        return getPool<T>().get(entity);
    }

    template<typename T>
    bool Registry::hasComponent(Entity entity) const {
        Signature id = ComponentIDGenerator::get<T>();
        if (id >= pools.size() || !pools[id]) return false;

        return pools[id]->hasComponent(entity);
    }
}
