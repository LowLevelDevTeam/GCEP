#pragma once
#include <Engine/Core/Entity-Component-System/headers/ComponentPool.hpp>
namespace gcep
{
    template <std::derived_from<Component> T>
    void ComponentPool<T>::addComponent(Entity entity)
    {
        if (entity >= m_sparse.size()){m_sparse.resize(entity +1, INVALID_VALUE );}
        m_sparse[entity] = static_cast<Index>(m_components.size());
        m_components.push_back(T{entity,ComponentIDGenerator::get<T>()});
        m_dense.push_back(entity);
    }

    template <std::derived_from<Component> T>
    void ComponentPool<T>::removeComponent(Entity entity)
    {
        if (!hasComponent(entity)) return;
        Index indexToRemove = m_sparse[entity];
        Entity lastEntityID = m_dense.back();
        m_dense[indexToRemove] = lastEntityID;
        m_components[indexToRemove] = std::move(m_components.back());
        m_sparse[lastEntityID] = indexToRemove;

        m_dense.pop_back();
        m_components.pop_back();

        m_sparse[entity] = INVALID_VALUE;
    }

    template <std::derived_from<Component> T>
    bool ComponentPool<T>::hasComponent(Entity entity) const
    {
        return entity < m_sparse.size() && m_sparse[entity] != INVALID_VALUE;

    }

    template<std::derived_from<Component> T>
    T& ComponentPool<T>::get(Entity entity) {
        return m_components[m_sparse[entity]];
    }

}