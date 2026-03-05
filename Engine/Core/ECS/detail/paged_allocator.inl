#pragma once

namespace gcep::ECS
{
    template<typename T>
    PagedAllocator<T>::~PagedAllocator()
    {
        for (auto page : m_pages)
        {
            if (page != nullptr) delete[] page;
        }
    }

    template<typename T>
    void PagedAllocator<T>::reserve(size_t entityCount)
    {
        size_t lastPageNeeded = entityCount >> PAGE_SHIFT;

        if (lastPageNeeded >= m_pages.size()) {
            m_pages.resize(lastPageNeeded + 1, nullptr);
        }

        for (size_t i = 0; i <= lastPageNeeded; ++i) {
            if (m_pages[i] == nullptr) {
                m_pages[i] = new T[PAGE_SIZE];
                std::fill(m_pages[i], m_pages[i] + PAGE_SIZE, m_defaultValue);
            }
        }
    }



    template<typename T>
    T& PagedAllocator<T>::get(size_t index)
    {
        size_t pageNumber = index >> PAGE_SHIFT;
        allocatePages(pageNumber);
        return m_pages[pageNumber][index & PAGE_MASK];
    }

    template<typename T>
    T PagedAllocator<T>::get(size_t index) const
    {
        size_t pageNumber = index >> PAGE_SHIFT;
        if (pageNumber >= m_pages.size() || m_pages[pageNumber] == nullptr) {
            return m_defaultValue;
        }
        return m_pages[pageNumber][index & PAGE_MASK];
    }


    template <typename T>
    void PagedAllocator<T>::allocatePages(size_t pageNumber)
    {
        if (pageNumber >= m_pages.size())
        {
            m_pages.resize(pageNumber + 1, nullptr);
        }

        if (m_pages[pageNumber] == nullptr)
        {
            m_pages[pageNumber] = new T[PAGE_SIZE];
            std::fill_n(m_pages[pageNumber], PAGE_SIZE, m_defaultValue);
        }
    }
}
