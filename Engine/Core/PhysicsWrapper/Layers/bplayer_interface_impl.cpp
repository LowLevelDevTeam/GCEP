#include "bplayer_interface_impl.hpp"

// STL
#include <string>

namespace gcep
{
    BPlayerInterfaceImpl::BPlayerInterfaceImpl()
    {
        m_ObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_ObjectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
    }

    glm::uint BPlayerInterfaceImpl::getNumBroadPhaseLayers() const
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer BPlayerInterfaceImpl::getBroadPhaseLayer(JPH::ObjectLayer layer) const
    {
        return m_ObjectToBroadPhase[layer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BPlayerInterfaceImpl::getBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
            case (JPH::BroadPhaseLayer::Type)0: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)1: return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
} // gcep