#pragma once

// Libs
#include "glm/fwd.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace gcep
{

    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr glm::uint NUM_LAYERS = 2;
    };

    class BPlayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPlayerInterfaceImpl();

        [[nodiscard]] glm::uint getNumBroadPhaseLayers() const override;

        [[nodiscard]] JPH::BroadPhaseLayer getBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        /// Get the user readable name of a broadphase layer (debugging purposes)
        [[nodiscard]] const char* getBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer m_ObjectToBroadPhase[Layers::NUM_LAYERS];
    };
}
