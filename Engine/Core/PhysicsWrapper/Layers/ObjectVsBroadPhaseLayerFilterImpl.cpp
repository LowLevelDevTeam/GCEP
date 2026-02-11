#include "ObjectVsBroadPhaseLayerFilterImpl.hpp"

#include "BPLayerInterfaceImpl.hpp"

namespace gcep {
    bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const
    {
        if (layer1 == Layers::NON_MOVING && layer2 == BroadPhaseLayers::NON_MOVING)
            return false;

        return true;
    }
} // gcep