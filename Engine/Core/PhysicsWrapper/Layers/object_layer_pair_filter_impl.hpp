#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace gcep
{
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        bool shouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
    };
} // gcep
