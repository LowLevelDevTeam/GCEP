#pragma once
#include <Engine/Core/ECS/headers/component_pool.hpp>
namespace gcep::ECS
{
    template <typename T>
    void ComponentPool<T>::addDefault(EntityID entity)
    {
      T Comp = add(entity);
    }

    template<typename T>
    template<typename ... Args>
    T & ComponentPool<T>::add(EntityID entity, Args &&...args)
    {
        if (hasComponent(entity))
        {
            Index denseIndex = m_sparse.get(entity);
            m_components[denseIndex] = T(std::forward<Args>(args)...);
            return m_components[denseIndex];
        }
        m_sparse.get(entity) = static_cast<Index>(m_components.size());
        m_dense.push_back(entity);
        m_components.emplace_back(std::forward<Args>(args)...);
        return m_components.back();
    }

    template <typename T>
    void ComponentPool<T>::removeComponent(EntityID entity)
    {
        Index indexToRemove = m_sparse.get(entity);
        if (indexToRemove == INVALID_VALUE) return;

        EntityID lastEntityID = m_dense.back();
        if (entity != lastEntityID) {
            m_dense[indexToRemove] = lastEntityID;
            m_components[indexToRemove] = std::move(m_components.back());
            m_sparse.get(lastEntityID) = indexToRemove;
        }

        m_dense.pop_back();
        m_components.pop_back();

        m_sparse.get(entity) = INVALID_VALUE;
    }

    template <typename T>
    bool ComponentPool<T>::hasComponent(EntityID entity) const
    {
        return m_sparse.get(entity) != INVALID_VALUE &&  m_sparse.get(entity) < m_dense.size() && m_dense[m_sparse.get(entity)] == entity;
    }

    template<typename T>
    T& ComponentPool<T>::get(EntityID entity) {
        return m_components[m_sparse.get(entity)];
    }

    template<typename T>
    const T & ComponentPool<T>::get(EntityID entity) const {
        return m_components[m_sparse.get(entity)];
    }

    template<typename T>
    void ComponentPool<T>::reserve(size_t n)  {
        m_sparse.reserve(n);
        m_dense.reserve(n);
        m_components.reserve(n);
    }

}