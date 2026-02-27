#pragma once
#include <bit>
#include <vector>

namespace gcep::ECS
{
    /**
     * @class PagedAllocator
     * @brief Memory allocator that organizes data into discrete memory blocks (pages).
     * @details This structure allows for quasi-contiguous data storage while avoiding
     * massive reallocations by allocating memory in fixed-size chunks as needed.
     */
    template <typename T>
    class PagedAllocator
    {
    public:
        /**
         * @brief Parameterized constructor.
         * @param defaultValue The default value used to initialize every
         * memory slot when a new page is allocated.
         */
        explicit PagedAllocator(T defaultValue) : m_defaultValue(defaultValue)
        {
        }

        /**
         * @brief Destructor.
         * Frees all allocated pages and deallocates the memory occupied
         * by the stored elements.
         */
        ~PagedAllocator();

        /**
         * @brief Pre-allocates memory for a given number of entities.
         * @param entityCount Total number of entities to support without
         * reallocating the page pointer vector.
         */
        void reserve(std::size_t entityCount);

        /**
         * @brief Read/Write access to an element by its index.
         * @details If the index points to an unallocated page, the memory
         * block is automatically created (lazy allocation).
         * @param index The position of the element in the allocator.
         * @return T& Reference to the stored element.
         */
        [[nodiscard]] T& get(std::size_t index);

        /**
         * @brief Read-only access to an element by its index.
         * @note If the page does not exist, returns the default value without
         * allocating new memory.
         * @param index The position of the element in the allocator.
         * @return T A copy of the element or m_defaultValue.
         */
        [[nodiscard]] T get(std::size_t index) const;

    private:
        /** @brief Page size (number of elements per memory block). Must be a power of 2. */
        static constexpr std::size_t PAGE_SIZE = 1024;
        /** @brief Bitwise shift used to calculate the page number from an index. */
        static constexpr std::size_t PAGE_SHIFT = std::countr_zero(PAGE_SIZE);
        /** @brief Bitwise mask used to retrieve the element's local index within its page. */
        static constexpr std::size_t PAGE_MASK = PAGE_SIZE - 1;

        std::vector<T*> m_pages; ///< Array of pointers to the memory blocks.
        T m_defaultValue;        ///< Initialization value for new memory slots.

        /**
         * @brief Ensures a specific memory block is allocated.
         * @details Checks if the page at pageNumber exists; if not, it allocates the
         * necessary memory and initializes it with the default value.
         * @param pageNumber The index of the memory block to allocate.
         */
        void allocatePages(std::size_t pageNumber);
    };
}

#include <Engine/Core/ECS/detail/paged_allocator.inl>
