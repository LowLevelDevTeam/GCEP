#pragma once
#include "EntityComponent.hpp"
#include <vector>


namespace gcep {
    struct IPool {
        virtual ~IPool() {}
        virtual void addComponent(EntityID entity) =0;
        virtual void removeComponent(EntityID entity) = 0;
        virtual bool hasComponent(EntityID entity) const =0;
        virtual const std::vector<EntityID>& getEntities() const = 0;
        virtual const size_t getSize() const = 0;
    };


    template<std::derived_from<Component> T>
    struct ComponentPool : public IPool
    {
    private:
        std::vector<EntityID> m_dense;
        std::vector<Index> m_sparse;
        std::vector<T> m_components;


    public:
        void addComponent(EntityID entity) override;

        void removeComponent(EntityID entity) override;

        bool hasComponent(EntityID entity) const override;

        T& get(EntityID entity);

        const std::vector<EntityID>& getEntities() const override { return m_dense; }

        const size_t getSize() const override { return m_dense.size(); }

    };

}
#include <Engine/Core/Entity-Component-System/detail/ComponentPool.inl>
