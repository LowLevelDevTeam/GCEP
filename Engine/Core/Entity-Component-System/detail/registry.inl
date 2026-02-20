#pragma once
#include <Engine/Core/Entity-Component-System/headers/registry.hpp>

namespace gcep
{
    inline EntityID Registry::createEntity()
    {
        if (!m_freeIDs.empty())
        {
            EntityID freeID = m_freeIDs.back();
            m_freeIDs.pop_back();
            m_entitySignatures.get(freeID).reset();
            return freeID;
        }
        EntityID newID  = m_nextId++;
        m_entitySignatures.get(newID);
        return newID;
    }

    inline void Registry::destroyEntity(EntityID entity) {
        m_entitiesToDestroy.push_back(entity);
    }

    inline void Registry::update() {
        for (auto toRemove : m_entitiesToDestroy) {
            removeEntity(toRemove);
        }
        m_entitiesToDestroy.clear();
    }

    template<typename T, typename... Args>
    T& Registry::addComponent(EntityID entityID, Args&&... args)
    {
        auto& signature = m_entitySignatures.get(entityID);
        const auto componentID = ComponentIDGenerator::get<T>();
        if (signature.test(componentID))
        {
            return getPool<T>().add(entityID, std::forward<Args>(args)...);
        }
        signature.set(componentID);
        return getPool<T>().add(entityID, std::forward<Args>(args)...);
    }

    template<typename T>
    void Registry::removeComponent(EntityID entityID)
    {
        getPool<T>().removeComponent(entityID);
        m_entitySignatures.get(entityID).reset(ComponentIDGenerator::get<T>());
    }

    template<typename T>
    T &Registry::getComponent(EntityID entity_id)
    {
        return getPool<T>().get(entity_id);
    }

    template<typename T>
    bool Registry::hasComponent(EntityID entity) const {
        return m_entitySignatures.get(entity).test(ComponentIDGenerator::get<T>());
    }

    inline const Signature Registry::getSignature(EntityID entity) const
    {
        return m_entitySignatures.get(entity);
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

    template<typename T>
    ComponentPool<T>& Registry::getPool()
    {
        uint32_t id = ComponentIDGenerator::get<T>();

        if (id >= m_pools.size()) {
            m_pools.resize(id + 1);
        }
        if (!m_pools[id]) {
            m_pools[id] = std::make_unique<ComponentPool<T>>();
        }

        return static_cast<ComponentPool<T>&>(*m_pools[id]);
    }

    inline void Registry::removeEntity(EntityID toRemove)
    {
        // 1. Utilisation de get() (version const)
        Signature& sig = m_entitySignatures.get(toRemove);
        if (sig.none()) return;
        unsigned long long mask = sig.to_ullong();
        while (mask > 0) {
            unsigned long poolIndex;
            if (_BitScanForward64(&poolIndex, mask)) {
                if (m_pools[poolIndex]) {
                    m_pools[poolIndex]->removeComponent(toRemove);
                }
                mask &= ~(1ULL << poolIndex);
            }
        }
        sig.reset();
        m_freeIDs.push_back(toRemove);
    }
}
