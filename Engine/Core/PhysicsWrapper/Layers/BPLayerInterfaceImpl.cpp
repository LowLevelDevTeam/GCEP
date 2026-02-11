#include "BPLayerInterfaceImpl.hpp"

namespace gcep
{
    BPLayerInterfaceImpl::BPLayerInterfaceImpl()
    {
        m_ObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_ObjectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
    }

    glm::uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
    {
        return m_ObjectToBroadPhase[layer];
    }
} // gcep