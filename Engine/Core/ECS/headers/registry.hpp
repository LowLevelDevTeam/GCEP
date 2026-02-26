#pragma once
#include "component_pool.hpp"
#include "entity_component.hpp"
#include "view_finder.hpp"
#include  "entity_component.hpp"

#include <memory>

/**
 * @namespace gcep
 * @brief Namespace for the Gaming Campus Engine Paris
 */
namespace gcep::ECS
{
    /**
     * @class Registry
     * @brief Central Manager for the ECS (Entity Component System).
     * @details The Registry handles the lifecycle of entities, paged allocation of signatures,
     * and maintains the link between entities and their respective components.
     */
    template<typename... Args> class View;

    class Registry {
    public:
        /**
         * @begin Default destructor of the class.
         */
        ~Registry() = default;

        /**
         * @brief Creates a new entity.
         * Recycles an ID from the free list if available; otherwise, increments the internal counter.
         * @return EntityID The unique identifier of the new entity.
         */
        [[nodiscard]] EntityID createEntity();

        /**
         * @brief Marks an entity for deferred destruction.
         * @note The entity remains valid until the next call to update().
         * @param entity The identifier of the entity to be removed.
         */
        void destroyEntity(EntityID entity);

        /**
         * @brief Synchronizes the ECS state.
         * Performs the actual destruction of marked entities, pool cleanup (swap-and-pop),
         * and ID recycling. Should be called at the end of every frame.
         */
        void update();

        /**
        * @brief Constructs and attaches a component of type T to an entity in-place.
        * @details Updates the entity's signature and forwards arguments to the pool's constructor.
        * @tparam T Component type.
        * @tparam Args Constructor argument types.
        * @param entityID Target entity ID.
        * @param args Arguments for perfect forwarding.
        */
        template<typename T, typename... Args>
        [[nodiscard]] T& addComponent(EntityID entityID, Args&&... args);

        /**
         * @brief Removes a component of type T from an entity.
         * @tparam T The type of component to remove.
         * @param entity The target entity ID.
         */
        template<typename T>
        void removeComponent(EntityID entity);

        /**
         * @brief Retrieves a reference to an entity's component.
         * @tparam T The type of component to retrieve.
         * @param entity The target entity ID.
         * @return T& Reference to the stored component data.
         */
        template<typename T>
        [[nodiscard]] T& getComponent(EntityID entity);

        /**
         * @brief Checks if an entity possesses a component of type T.
         * @tparam T The component type to test for.
         * @param entity The target entity ID.
         * @return true if the component is present, false otherwise.
         */
        template<class T>
        [[nodiscard]] bool hasComponent(EntityID entity) ;

        /**
         * @brief Creates an inclusion-filtered view.
         * Returns entities possessing AT LEAST the specified component types.
         * @tparam Args List of required component types.
         * @return View<Args...> An iterable object containing matching entities.
         */
        template<typename... Args>
        [[nodiscard]] View<Args...> view();

        /**
         * @brief Retrieves the storage pool associated with a component type.
         * @tparam T The component type.
         * @return ComponentPool<T>& Reference to the specialized component pool.
         */
        template<typename T>
        [[nodiscard]] ComponentPool<T>& getPool();

    private:
        EntityIDGenerator m_idGenerator;
        std::vector<std::unique_ptr<IPool>> m_pools; ///< List of type-erased component m_pools.
        std::vector<EntityID> m_entitiesToDestroy;   ///< Queue for deferred destruction.

        /** @brief Internal logic for removing an entity and its associated data. */
        void removeEntity(EntityID toRemove);
    };

}

#include <Engine/Core/ECS/detail/registry.inl>
#include <Engine/Core/ECS/detail/view_finder.inl>
