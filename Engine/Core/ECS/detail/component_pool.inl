#pragma once

#ifdef __GNUG__
    #include <cxxabi.h>
    #include <cstdlib>
#endif

namespace gcep::ECS
{
    template <ComponentConcept  T>
    void ComponentPool<T>::addDefault(EntityID entity)
    {
        T Comp = add(entity);
    }

    template<ComponentConcept  T>
    template<typename ... Args>
    T & ComponentPool<T>::add(EntityID entity, Args &&...args)
    {
        if (hasComponent(entity))
        {
            Index denseIndex = m_sparse.get(entity & VALUE_MASK);
            m_components[denseIndex] = T(std::forward<Args>(args)...);
            return m_components[denseIndex];
        }
        m_sparse.get(entity & VALUE_MASK) = static_cast<Index>(m_components.size());
        m_dense.push_back(entity);
        m_components.emplace_back(std::forward<Args>(args)...);
        return m_components.back();
    }

    template <ComponentConcept  T>
    void ComponentPool<T>::removeComponent(EntityID entity)
    {
        Index indexToRemove = m_sparse.get(entity & VALUE_MASK);
        if (indexToRemove == INVALID_VALUE) return;
        if (m_dense[indexToRemove] != entity) return;

        EntityID lastEntityID = m_dense.back();
        if (entity != lastEntityID) {
            m_dense[indexToRemove] = lastEntityID;
            m_components[indexToRemove] = std::move(m_components.back());
            m_sparse.get(lastEntityID & VALUE_MASK) = indexToRemove;
        }

        m_dense.pop_back();
        m_components.pop_back();

        m_sparse.get(entity & VALUE_MASK) = INVALID_VALUE;
    }

    template <ComponentConcept  T>
    bool ComponentPool<T>::hasComponent(EntityID entity) const
    {
        return m_sparse.get(entity & VALUE_MASK) != INVALID_VALUE &&  m_sparse.get(entity& VALUE_MASK) < m_dense.size() && m_dense[m_sparse.get(entity& VALUE_MASK)] == entity;
    }

    template<ComponentConcept  T>
    T& ComponentPool<T>::get(EntityID entity) {
        Index idx = m_sparse.get(entity & VALUE_MASK);
        if (idx == INVALID_VALUE || idx >= m_components.size() || m_dense[idx] != entity) {
            throw std::out_of_range("ComponentPool::get: entity does not have component or index out of range");
        }
        return m_components[idx];
    }

    template<ComponentConcept  T>
    const T & ComponentPool<T>::get(EntityID entity) const {
        Index idx = m_sparse.get(entity & VALUE_MASK);
        if (idx == INVALID_VALUE || idx >= m_components.size() || m_dense[idx] != entity) {
            throw std::out_of_range("ComponentPool::get: entity does not have component or index out of range");
        }
        return m_components[idx];
    }

    template<ComponentConcept  T>
    void ComponentPool<T>::reserve(size_t n)  {
        m_sparse.reserve(n);
        m_dense.reserve(n);
        m_components.reserve(n);
    }

    template<ComponentConcept T>
    std::string ComponentPool<T>::getName() const {
        #ifdef __GNUG__
                int status = 0;
                char* demangled = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
                if (status == 0)
                {
                    std::string result(demangled);
                    free(demangled);
                    return result;
                }
                return typeid(T).name();
        #else
                return typeid(T).name();
        #endif
    }

    template<ComponentConcept T>
    void ComponentPool<T>::serializeEntity(EntityID entity, SER::IArchive &archive)
    {
        const T& component = get(entity);
        SER::BinarySerializer<T>::serialize(archive, component);
    }

    template<ComponentConcept T>
    void ComponentPool<T>::deserializeEntity(EntityID entity, SER::IArchive &archive)
    {
        T component = SER::BinarySerializer<T>::deserialize(archive);
        add(entity, std::move(component));
    }
} // namespace gcep::ECS
