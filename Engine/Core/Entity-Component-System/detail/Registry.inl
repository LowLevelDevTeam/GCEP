#pragma once
#include <Engine/Core/Entity-Component-System/headers/Registry.hpp>
#include <Engine/Core/Entity-Component-System/headers/View.hpp>

namespace gcep
{
    inline EntityID Registry::createEntity()
    {
        if (!freeIDs.empty())
        {
            EntityID freeID = freeIDs.back();
            freeIDs.pop_back();
            entitySignatures.get(freeID).reset();
            return freeID;
        }
        EntityID newID  = nextId++;


        entitySignatures.get(newID);
        return newID;
    }

    inline void Registry::destroyEntity(EntityID entity) {
        entitiesToDestroy.push_back(entity);
    }

    inline void Registry::update() {
        for (auto toRemove : entitiesToDestroy) {
            removeEntity(toRemove);
        }
        entitiesToDestroy.clear();
    }

    inline void Registry::removeEntity(EntityID toRemove)
    {
        // 1. Utilisation de get() (version const)
        Signature& sig = entitySignatures.get(toRemove);
        if (sig.none()) return;


        unsigned long long mask = sig.to_ullong();

        while (mask > 0) {
            unsigned long poolIndex;
            if (_BitScanForward64(&poolIndex, mask)) {
                if (pools[poolIndex]) {
                    pools[poolIndex]->removeComponent(toRemove);
                }
                mask &= ~(1ULL << poolIndex);
            }
        }

        sig.reset();
        freeIDs.push_back(toRemove);
    }

    template<typename T>
    ComponentPool<T>& Registry::getPool()
    {
        uint32_t id = ComponentIDGenerator::get<T>();

        if (id >= pools.size()) {
            pools.resize(id + 1);
        }
        if (!pools[id]) {
            pools[id] = std::make_unique<ComponentPool<T>>();
        }

        return static_cast<ComponentPool<T>&>(*pools[id]);
    }

    template<typename T>
    void Registry::addComponent(EntityID entityID)
    {
        getPool<T>().addComponent(entityID);
        entitySignatures.get(entityID).set(ComponentIDGenerator::get<T>());
    }

    template<typename T>
    void Registry::removeComponent(EntityID entityID)
    {
        getPool<T>().removeComponent(entityID);
        entitySignatures.get(entityID).reset(ComponentIDGenerator::get<T>());
    }

    template<typename T>
    T &Registry::getComponent(EntityID entity_id)
    {
        return getPool<T>().get(entity_id);
    }

    template<typename T>
    bool Registry::hasComponent(EntityID entity) const {
        return entitySignatures.get(entity).test(ComponentIDGenerator::get<T>());
    }

    inline const Signature Registry::getSignature(EntityID entity) const
    {
        return entitySignatures.get(entity);
    }

    template<typename... Args>
    View<Args...> Registry::partialView()
    {
        return View<Args...>(*this, false);
    }

    template<typename ... Args>
    View<Args...> Registry::exactView()
    {
        return View<Args...>(*this, true);
    }


}
