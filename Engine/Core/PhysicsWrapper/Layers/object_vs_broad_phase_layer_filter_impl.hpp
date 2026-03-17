#pragma once

// Externals
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace gcep
{
    /**
     * @class ObjectVsBroadPhaseLayerFilterImpl
     * @brief Implementation of Jolt's ObjectVsBroadPhaseLayerFilter.
     *
     * Determines whether an object on a specific object layer should collide
     * with a given broadphase layer. Used during broadphase collision filtering
     * to optimize physics simulation.
     */
    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        /**
         * @brief Checks if an object layer should collide with a broadphase layer.
         * @param layer1 The object layer of the physics body.
         * @param layer2 The broadphase layer.
         * @return true if the object should participate in collisions with the given broadphase layer, false otherwise.
         */
        bool shouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override;
    };
} // namespace gcep
