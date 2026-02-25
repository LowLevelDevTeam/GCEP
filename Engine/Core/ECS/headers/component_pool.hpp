#pragma once
#include "paged_allocator.hpp"
#include "entity_component.hpp"

#include <vector>

/**
 * @namespace gcep
 * @brief Namespace for the Gaming Campus Engine Paris
 */
namespace gcep::ECS
{
    /**
     * @struct IPool
     * @brief Virtual interface allowing interactions with different types of component pools.
     */
    struct IPool
    {
        /// @brief Default virtual destructor.
        virtual ~IPool()
        {
        }

        /**
         * @brief Allocates and attaches a new component to an entity.
         * @note This method is used by the Registry for Type Erasure.
         * It ensures internal storage is updated for the given entity.
         * @param entity The ID of the entity that will receive the component.
         */
        virtual void addDefault(EntityID entity) = 0;

        /**
         * @brief Removes a component from an entity and destroys it.
         * @note This method is used by the Registry for Type Erasure.
         * It ensures internal storage is updated for the given entity.
         * @param entity The ID of the entity from which the component should be removed.
         */
        virtual void removeComponent(EntityID entity) = 0;

        /**
         * @brief Checks if an entity possesses this type of component.
         * @note This method allows for generic testing of a component's existence.
         * It is particularly useful for filtering systems and Views.
         * @param entity The ID of the entity to test.
         * @return true if the entity has the component associated with this pool, false otherwise.
         */
        [[nodiscard]] virtual bool hasComponent(EntityID entity) const = 0;

        /**
         * @brief Getter providing read-only access to the Dense Array.
         * @note Contains all entities possessing this component type. Primarily used
         * by Views to optimize iterations.
         * @return const std::vector<EntityID>& A reference to the dense vector.
         */
        virtual const std::vector<EntityID>& getEntities() const = 0;

        /**
         * @brief Pre-allocates memory to accommodate n components.
         * @note Used to avoid costly reallocations and memory fragmentation
         * during batch entity creation. Reserves space for both components and dense IDs.
         * @param n Number of elements to reserve space for.
         */
        virtual void reserve(size_t n) = 0;
    };

    /**
     * @class ComponentPool
     * @brief Component pool implementation based on a Sparse Set data structure.
     * @details Manages storage and lifecycle for components of type T.
     * Uses three synchronized structures for optimal performance:
     * - A Paged Sparse Array for O(1) direct access via EntityID.
     * - A Dense Array for grouping active entities to allow fast iteration.
     * - A contiguous data array to maximize CPU cache efficiency.
     * @tparam T The type of component stored in this pool.
     */
    template<typename T>
    struct ComponentPool : public IPool
    {
    public:
        /**
         * @brief Adds a default-initialized component of type T to the entity.
         * @note Overrides the virtual IPool interface. This allows the Registry to add
         * components generically via Type Erasure.
         * @param entity The ID of the entity receiving the component.
         */
        void addDefault(EntityID entity) override;

        /**
         * @brief Constructs and attaches a component of type T in-place using provided arguments.
         * @details Uses variadic templates and perfect forwarding to invoke the constructor
         * of T directly within the pool's storage. This is the most efficient way to
         * initialize components with specific data, as it eliminates temporary copies.
         * @tparam Args Variadic types for the constructor arguments.
         * @param entity The unique identifier of the target entity.
         * @param args The arguments to be forwarded to the component's constructor.
         * @return T& A reference to the newly created component.
         */
        template<typename... Args>
        [[nodiscard]] T& add(EntityID entity, Args&&... args);

        /**
         * @brief Removes the component of type T from the entity.
         * @note Uses the "Swap and Pop" algorithm to maintain data contiguity
         * within the Dense Array.
         * @param entity The target entity ID.
         */
        void removeComponent(EntityID entity) override;

        /**
         * @brief Checks if the entity has the component of type T.
         * @note Validates existence in the Sparse Array (PagedAllocator) and confirms
         * a match in the Dense Array to prevent false positives.
         * @param entity The entity ID to test.
         * @return true if the entity has the component and the memory link is valid.
         */
        [[nodiscard]] bool hasComponent(EntityID entity) const override;

        /**
         * @brief Retrieves a reference to the component of type T for the chosen entity.
         * @note O(1) direct access via the Sparse Array. Designed for maximum performance;
         * contains no internal safety checks.
         * @warning The user must ensure the entity has the component (via hasComponent)
         * before calling this, otherwise Undefined Behavior (UB) will occur.
         * @param entity The target entity ID.
         * @return T& Reference to the entity's component.
         */
        [[nodiscard]] T& get(EntityID entity);

        /**
         * @brief Retrieves a read-only reference to the component of type T.
         * @note Constant version of get() allowing data access without modification.
         * @param entity The target entity ID.
         * @return const T& Constant reference to the component.
         */
        [[nodiscard]] const T& get(EntityID entity) const;

        /**
         * @brief Pre-allocates memory to accommodate n components of type T.
         * @note Reserves space in both Dense and Component vectors.
         * @warning This does not affect the PagedAllocator (Sparse), which manages
         * its own memory dynamically.
         * @param n Total number of components to reserve space for.
         */
        void reserve(size_t n) override;

        /**
         * @brief Returns a constant reference to the Dense Array of entities.
         * @note This list is guaranteed to be contiguous, allowing for ultra-fast,
         * cache-friendly CPU iterations.
         */
        [[nodiscard]] const std::vector<EntityID>& getEntities() const override
        {
            return m_dense;
        }

    private:
        /**
         * @brief Maps an EntityID to its index in the Dense Array.
         * Allows O(1) access while handling high EntityIDs in a memory-efficient way.
         */
        PagedAllocator<Index> m_sparse{ std::numeric_limits<Index>::max() };

        /**
         * @brief Compact list of EntityIDs possessing this component.
         * Maintained contiguously to guarantee fast iterations.
         */
        std::vector<EntityID> m_dense;

        /**
         * @brief Contiguous storage for component data of type T.
         * Aligned with m_dense to maximize L1/L2 cache performance.
         */
        std::vector<T> m_components;
    };
}

#include <Engine/Core/ECS/detail/component_pool.inl>
