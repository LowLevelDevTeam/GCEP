#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace gcep {
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter{
        public:

        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
    };
} // gcep
