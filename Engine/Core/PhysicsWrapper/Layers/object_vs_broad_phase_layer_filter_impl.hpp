#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace gcep
{
    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool shouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override;

    };
} // gcep
