#pragma once
#include <Engine/Core/Entity-Component-System/headers/view_finder.hpp>

namespace gcep {

#pragma region Iterator Function
    template<typename... Args>
    View<Args...>::Iterator::Iterator(const EntityID* current, const EntityID* last,  View& v)
    : m_currentEntity(current), m_lastEntity(last), m_view(v)
    {
        if (m_currentEntity && m_currentEntity != m_lastEntity && !m_view.match(*m_currentEntity))
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
        while (m_currentEntity != m_lastEntity && !m_view.match(*m_currentEntity));

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
    View<Args...>::View(Registry& registry) : m_registry(registry), m_smallestPool(nullptr)
    {

        m_pools = std::make_tuple(&m_registry.getPool<Args>()...);

        size_t minSize = std::numeric_limits<size_t>::max();
        auto findSmallest = [&](IPool* pool) {
            if (!pool)
            {
                m_smallestPool = nullptr;
                return;
            }
            if (pool->getEntities().size() < minSize) {
                minSize = pool->getEntities().size();
                m_smallestPool = pool;
            }
        };

       (findSmallest(static_cast<IPool*>(&m_registry.getPool<Args>())), ...);
    }

    template<typename ... Args>
    bool View<Args...>::match(EntityID entity) const
    {
        return std::apply([&](auto&&... pools) {
            return ((pools == m_smallestPool ? true :pools->hasComponent(entity)) && ...);
        }, m_pools);
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
