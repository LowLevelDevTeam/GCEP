#pragma once
#include <vector>

namespace gcep {
    template <typename T>
    class PagedAllocator

    {
        static constexpr size_t PAGE_SIZE = 1024;
        static constexpr size_t PAGE_SHIFT = std::countr_zero(PAGE_SIZE);
        static constexpr size_t PAGE_MASK = PAGE_SIZE - 1;
        std::vector<T*> m_pages;
        T m_defaultValue;

        void allocatePages(size_t pageNumber);

    public:

        explicit PagedAllocator(T defaultValue) : m_defaultValue(defaultValue) {}
        ~PagedAllocator();


        T& get(size_t index) ;
        T get(size_t index) const;

        void reserve(size_t entityCount);

        T* getPage(size_t pageNumber);
        bool hasPage(size_t pageNumber) const;

    };
}

#include <Engine/Core/Entity-Component-System/detail/PagedAllocator.inl>