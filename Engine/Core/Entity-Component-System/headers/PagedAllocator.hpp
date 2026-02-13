#pragma once
#include <vector>


template <typename T>
class PagedAllocator

{
    static constexpr size_t PAGE_SIZE = 4096;
    std::vector<T*> m_pages;

    void allocatePages(size_t pageNumber);






};