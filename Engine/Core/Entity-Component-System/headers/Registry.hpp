#pragma once
#include "EntityComponent.hpp"
#include "ComponentPool.hpp"
#include "PagedAllocator.hpp"
#include <intrin.h>
#include <memory>



namespace gcep {
template<typename... Args> class View;

    class Registry {
    public:
        std::vector<std::unique_ptr<IPool>> pools;
        std::vector<EntityID> freeIDs;
        std::vector<EntityID> entitiesToDestroy;
        EntityID nextId = 0;
        PagedAllocator<Signature> entitySignatures;

        Registry() : entitySignatures(Signature{}) {}

        EntityID createEntity();

        void destroyEntity(EntityID entity);

        void update();

        void removeEntity(EntityID toRemove);

        template<typename T>
        ComponentPool<T>& getPool();



        template<typename T>
        void addComponent(EntityID entityID);


        template<typename T>
        void removeComponent(EntityID entity);

        template<typename T>
        T& getComponent(EntityID entity);

        template<class T>
        bool hasComponent(EntityID entity) const;

        const Signature getSignature(EntityID entity) const;

        template<typename... Args>
        View<Args... > partialView();

        template<typename... Args>
        View<Args...> exactView();


    };
}
#include <Engine/Core/Entity-Component-System/detail/Registry.inl>
