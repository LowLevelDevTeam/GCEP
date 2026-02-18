#pragma once
#include <cstdint>
#include <numeric>
#include <bitset>

namespace gcep
{
    using EntityID = std::uint32_t;
    inline EntityID INVALID_VALUE = std::numeric_limits<uint32_t>::max();
    constexpr size_t MAX_COMPONENTS = 256;
    using Signature = std::bitset<MAX_COMPONENTS>;

    using Index = uint32_t;


    struct Component
    {
        EntityID entityID = INVALID_VALUE;
    };


    class ComponentIDGenerator {
    private:
        static inline uint32_t m_nextID = 0;

    public:
        template<typename T>
        static uint32_t get()
    	{
        	static uint32_t id = m_nextID++;
        	return id;
    	}
    };
}
