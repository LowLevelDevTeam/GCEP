#include "BPLayerInterfaceImpl.hpp"

#include <string>

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

    const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
            case (JPH::BroadPhaseLayer::Type)0: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)1: return "MOVING";
            default: return "UNKNOWN";
        }
    }
} // gcep