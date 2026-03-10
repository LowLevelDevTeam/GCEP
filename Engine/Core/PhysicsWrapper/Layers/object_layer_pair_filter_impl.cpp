#include "object_layer_pair_filter_impl.hpp"

// Core
#include "bplayer_interface_impl.hpp"

namespace gcep
{
    bool ObjectLayerPairFilterImpl::shouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
    {
        if (a == Layers::NON_MOVING && b == Layers::NON_MOVING)
            return false;

        return true;
    }
} // gcep
