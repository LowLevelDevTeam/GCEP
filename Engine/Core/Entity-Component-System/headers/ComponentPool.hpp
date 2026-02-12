#pragma once
#include "EntityComponent.hpp"
#include <vector>


namespace gcep {
    struct IPool {
        virtual ~IPool() {}
        virtual void addComponent(Entity entity) =0;
        virtual void removeComponent(Entity entity) = 0;
        virtual bool hasComponent(Entity entity) const =0;
    };


    template<std::derived_from<Component> T>
    struct ComponentPool : public IPool
    {
    private:
        std::vector<Entity> m_dense;
        std::vector<Index> m_sparse;
        std::vector<T> m_components;

    public:
        void addComponent(Entity entity) override;

        void removeComponent(Entity entity) override;
²
        bool hasComponent(Entity entity) const override;

        T& get(Entity entity);
    };

}
#include <Engine/Core/Entity-Component-System/detail/ComponentPool.inl>
