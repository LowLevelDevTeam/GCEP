#pragma once
#include "entity_component.hpp"

#include <tuple>
#include <vector>

namespace gcep
{
    class Registry;

    /**
     * @class View
     * @brief Query system used to iterate over entities possessing a specific set of components.
     * @details The View is the primary tool for entity filtering. It optimizes iteration by
     * identifying the smallest (most restrictive) pool to minimize the number of checks performed.
     * @tparam Args List of component types required for filtering.
     */
    template <typename... Args>
    class View
    {
    public:
        /**
         * @brief Constructor for the View class.
         * @note Retrieves pool pointers and calculates the view signature upon construction
         * to maximize performance during iteration.
         * @param registry Reference to the parent Registry.
         * @param exact If true, the entity must possess EXCLUSIVELY the components in Args.
         * If false (default), it must possess AT LEAST the components in Args.
         */
        View(Registry& registry, bool exact);

        /**
         * @brief Checks if an entity meets the View's criteria.
         * @param entity The unique identifier of the entity to test.
         * @return true if the entity has the required components (respecting the exactness rule),
         * false otherwise.
         */
        [[nodiscard]] bool match(EntityID entity) const;

        /**
         * @struct Iterator
         * @brief Custom iterator for traversing entities validated by the View.
         * @details This iterator traverses the dense array of the smallest pool contiguously
         * and automatically skips entities that fail the match() test.
         */
        struct Iterator
        {
            const EntityID* m_currentEntity; ///< Pointer to the current entity in the dense array.
            const EntityID* m_lastEntity;    ///< Array boundary for end-of-iteration detection.
            View& m_view;                      ///< Reference to the parent m_view for match() calls.

            /**
             * @brief Initializes the iterator and advances to the first valid entity.
             */
            Iterator(const EntityID *current, const EntityID *last,  View &v);

            /**
             * @brief Advances to the next valid entity (pre-increment).
             * @return Iterator& Reference to the advanced iterator.
             */
            Iterator& operator++();

            /**
             * @brief Dereference operator to retrieve the current entity's ID.
             * @return EntityID The identifier of the entity.
             */
            EntityID operator*() const;

            /**
             * @brief Comparison operator for loop termination.
             */
            bool operator!=(const Iterator& other) const;
        };

        /**
         * @brief Returns the beginning iterator.
         */
        [[nodiscard]] Iterator begin();

        /**
         * @brief Returns the end iterator (sentinel).
         */
        Iterator end();

        /**
         * @brief Retrieves a specific component for the entity being iterated.
         * @note This method uses pool pointers cached in the m_pools tuple
         * to avoid expensive lookups in the Registry.
         * @tparam T The type of component to retrieve (must be part of Args).
         * @param entity The target entity ID.
         * @return T& Reference to the component data.
         */
        template<class T>
        [[nodiscard]] T &get(EntityID entity);

    private:
        Registry& m_registry;                  ///< Reference to the central Registry.
        Signature m_viewSignature;             ///< Binary signature pre-calculated for this view.
        IPool* m_smallestPool = nullptr;       ///< Pool used as the iteration base (optimization).
        std::tuple<ComponentPool<Args>*...> m_pools; ///< Cached pointers to component pools.
        bool m_isExact;                        ///< Filtering mode (inclusive or exclusive).
    };
}

