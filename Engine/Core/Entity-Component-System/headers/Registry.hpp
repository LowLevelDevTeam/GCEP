#pragma once
#include "EntityComponent.hpp"
#include "ComponentPool.hpp"
#include <memory>


namespace gcep {
    class Registry {
    public:
        std::vector<std::unique_ptr<IPool>> pools;
        std::vector<Entity> allEntities;
        std::vector<Entity> freeIDs;

        Entity createEntity();

        void removeEntity(Entity toRemove);

        template<typename T>
        ComponentPool<T>& getPool();



        template<typename T>
        void addComponent(Entity entityID);


        template<typename T>
        void removeComponent(Entity entity);

        template<typename T>
        T& getComponent(Entity entity);

        template<class T>
        bool hasComponent(Entity entity) const;
    };
}
#include <Engine/Core/Entity-Component-System/detail/Registry.inl>
