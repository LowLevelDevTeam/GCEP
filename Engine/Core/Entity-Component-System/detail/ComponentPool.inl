#pragma once
#include <Engine/Core/Entity-Component-System/headers/ComponentPool.hpp>
namespace gcep
{
    template <std::derived_from<Component> T>
    void ComponentPool<T>::addComponent(EntityID entity)
    {
        m_sparse.get(entity) = static_cast<Index>(m_components.size());
        m_components.push_back(T{entity, ComponentIDGenerator::get<T>()});
        m_dense.push_back(entity);
    }

    template <std::derived_from<Component> T>
    void ComponentPool<T>::removeComponent(EntityID entity)
    {
        Index indexToRemove = m_sparse.get(entity);
        if (indexToRemove == INVALID_VALUE) return;

        EntityID lastEntityID = m_dense.back();
        m_dense[indexToRemove] = lastEntityID;
        m_components[indexToRemove] = std::move(m_components.back());
        m_sparse.get(lastEntityID) = indexToRemove;

        m_dense.pop_back();
        m_components.pop_back();

        m_sparse.get(entity) = INVALID_VALUE;
    }

    template <std::derived_from<Component> T>
    bool ComponentPool<T>::hasComponent(EntityID entity) const
    {
        return m_sparse.get(entity) != INVALID_VALUE;;
    }

    template<std::derived_from<Component> T>
    T& ComponentPool<T>::get(EntityID entity) {
        Index index = m_sparse.get(entity);
        return m_components[index];
    }

    template<std::derived_from<Component> T>
    void ComponentPool<T>::reserve(size_t n)  {
        m_sparse.reserve(n);
        m_dense.reserve(n);
        m_components.reserve(n);
    }

}