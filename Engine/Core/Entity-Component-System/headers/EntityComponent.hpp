#pragma once
#include <cstdint>
#include <numeric>

namespace gcep
{
    using Entity = std::uint32_t;
    inline Entity INVALID_VALUE = std::numeric_limits<uint32_t>::max();
    using Signature = uint32_t;
    using Index = uint32_t;


    struct Component
    {
        Entity entityID = INVALID_VALUE;
        Signature signature;
    };


    class ComponentIDGenerator {
    private:
        static inline Signature m_nextID = 0;

    public:
        template<typename T>
        static Signature get()
    	{
        	static Signature id = m_nextID++;
        	return id;
    	}
    };
}
