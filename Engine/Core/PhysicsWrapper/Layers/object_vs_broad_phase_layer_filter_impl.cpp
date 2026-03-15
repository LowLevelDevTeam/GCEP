#include "object_vs_broad_phase_layer_filter_impl.hpp"

// Internals
#include "bplayer_interface_impl.hpp"

namespace gcep
{
    bool ObjectVsBroadPhaseLayerFilterImpl::shouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const
    {
        if (layer1 == Layers::NON_MOVING && layer2 == BroadPhaseLayers::NON_MOVING)
            return false;

        return true;
    }
} // namespace gcep
