#pragma once
#include <Engine/Core/Entity-Component-System/headers/View.hpp>

#include "Engine/Core/Entity-Component-System/headers/Registry.hpp"

namespace gcep {

#pragma region Iterator Function
    template<typename... Args>
    View<Args...>::Iterator::Iterator(const EntityID* current, const EntityID* last,  View& v)
    : m_currentEntity(current), m_lastEntity(last), view(v)
    {
        if (m_currentEntity && m_currentEntity != m_lastEntity && !view.match(*m_currentEntity))
        {
            operator++();
        }
    }


    template<typename... Args>
    View<Args...>::Iterator& View<Args...>::Iterator::operator++()
    {
        do {
        m_currentEntity++;
        }
        while (m_currentEntity != m_lastEntity && !view.match(*m_currentEntity));

        return *this;
    }

    template<typename... Args>
    EntityID View<Args...>::Iterator::operator*() const
    {
        return *m_currentEntity;
    }

    template<typename... Args>
    bool View<Args...>::Iterator::operator!=( const Iterator& other) const
    {
        return m_currentEntity != other.m_currentEntity;
    }

#pragma endregion

#pragma region View
    template<typename... Args>
    View<Args...>::View(Registry& registry, bool exact) : m_registry(registry), m_smallestPool(nullptr), m_isExact(exact)
    {

        m_pools = std::make_tuple(&m_registry.getPool<Args>()...);

        (m_viewSignature.set(ComponentIDGenerator::get<Args>()),...);

        size_t minSize = std::numeric_limits<size_t>::max();
        auto findSmallest = [&](IPool* pool) {
            if (!pool)
            {
                m_smallestPool = nullptr;
                return;
            }
            if (pool->getSize() < minSize) {
                minSize = pool->getSize();
                m_smallestPool = pool;
            }
        };

       (findSmallest(static_cast<IPool*>(&m_registry.getPool<Args>())), ...);
    }

    template<typename ... Args>
    bool View<Args...>::match(EntityID entity) const
    {
        const Signature entitySig = m_registry.getSignature(entity);

        if (m_isExact)
        {
            return entitySig == m_viewSignature;
        }
        else
        {
            return (entitySig & m_viewSignature) == m_viewSignature;
        }
    }


    template<typename ... Args>
    typename View<Args...>::Iterator View<Args...>::begin()
    {
        if (!m_smallestPool) {
            return Iterator{nullptr, nullptr, *this};
        }
        const auto& entities = m_smallestPool->getEntities();
        return Iterator{entities.data(), entities.data() + entities.size(), *this};
    }



    template<typename ... Args>
    typename View<Args...>::Iterator View<Args...>::end()
    {
        if (!m_smallestPool) {
            return Iterator{nullptr, nullptr, *this};
        }

        const auto& entities = m_smallestPool->getEntities();


        const EntityID* endPtr = entities.data() + entities.size();
        return Iterator{ endPtr, endPtr, *this };
    }

    template<typename ... Args>
    template<typename T>
    T& View<Args...>::get(EntityID entity) {
        return std::get<ComponentPool<T>*>(m_pools)->get(entity);
    }

#pragma endregion
}
