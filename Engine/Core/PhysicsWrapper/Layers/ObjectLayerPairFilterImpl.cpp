#include "ObjectLayerPairFilterImpl.hpp"

#include "BPLayerInterfaceImpl.hpp"

namespace gcep {
    bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
    {
        if (a == Layers::NON_MOVING && b == Layers::NON_MOVING)
            return false;

        return true;
    }
} // gcep