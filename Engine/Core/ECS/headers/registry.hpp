#pragma once

// Internals
#include <ECS/headers/component_pool.hpp>
#include <ECS/headers/entity_component.hpp>
#include <ECS/headers/view_finder.hpp>
#include <Log/log.hpp>

// STL
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
        Registry() { m_pools.resize(ComponentIDGenerator::count()); }
        ~Registry() = default;

        /**
         * @brief Creates a new entity.
         * @details Uses the EntityIDGenerator to allocate a new entity ID. If a destroyed entity
         * slot is available in the free list, that ID is reused with an incremented version number.
         * Otherwise, a new slot is allocated from the entity pool.
         * @return EntityID The unique identifier of the new entity.
         */
        EntityID createEntity();


        void createEntityWithID(EntityID entity);

        /**
         * @brief Marks an entity for deferred destruction.
         * @details The entity is queued for removal and its ID is invalidated through version
         * checking by the EntityIDGenerator. The entity remains logically valid until the next
         * call to update(), where it is permanently removed and its slot recycled.
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
        template<ComponentConcept T, typename... Args>
        T& addComponent(EntityID entityID, Args&&... args);

        /**
         * @brief Removes a component of type T from an entity.
         * @tparam T The type of component to remove.
         * @param entity The target entity ID.
         */
        template<ComponentConcept T>
        void removeComponent(EntityID entity);

        /**
         * @brief Retrieves a reference to an entity's component.
         * @tparam T The type of component to retrieve.
         * @param entity The target entity ID.
         * @return T& Reference to the stored component data.
         */
        template<ComponentConcept T>
        T& getComponent(EntityID entity);

        /**
         * @brief Checks if an entity possesses a component of type T.
         * @tparam T The component type to test for.
         * @param entity The target entity ID.
         * @return true if the component is present, false otherwise.
         */
        template<ComponentConcept T>
        [[nodiscard]] bool hasComponent(EntityID entity) ;

        /**
         * @brief Creates an inclusion-filtered view.
         * Returns entities possessing AT LEAST the specified component types.
         * @tparam Args List of required component types.
         * @return View<Args...> An iterable object containing matching entities.
         */
        template<ComponentConcept... Args>
        [[nodiscard]] View<Args...> view();

        /**
         * @brief Retrieves the storage pool associated with a component type.
         * @tparam T The component type.
         * @return ComponentPool<T>& Reference to the specialized component pool.
         */
        template<ComponentConcept T>
        [[nodiscard]] ComponentPool<T>& getPool();

        [[nodiscard]] const std::vector<std::unique_ptr<IPool>>& getPools() const;

        [[nodiscard]] const std::vector<uint32_t>& getTypeList() const;

    private:
        EntityIDGenerator m_idGenerator; ///< Generator for entity ID allocation, validation, and recycling with versioning support (provides generateID(), isValid(), destroyEntity()).
        std::vector<std::unique_ptr<IPool>> m_pools; ///< List of type-erased component m_pools.
        std::vector<EntityID> m_entitiesToDestroy;   ///< Queue for deferred destruction.
        std::vector<uint32_t> m_componentTypeList;

        /** @brief Internal logic for removing an entity and its associated data. */
        void removeEntity(EntityID toRemove);
    };
} // namespace gcep::ECS

#include <Engine/Core/ECS/detail/registry.inl>
#include <Engine/Core/ECS/detail/view_finder.inl>
