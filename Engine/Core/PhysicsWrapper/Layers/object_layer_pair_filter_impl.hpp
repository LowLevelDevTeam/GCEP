#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace gcep
{
    /**
     * @class ObjectLayerPairFilterImpl
     * @brief Implementation of Jolt's ObjectLayerPairFilter.
     *
     * Determines whether two object layers should collide during physics simulation.
     * Typically used to filter collisions between static and dynamic objects.
     */
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        /**
         * @brief Checks if two object layers should collide.
         * @param a First object layer.
         * @param b Second object layer.
         * @return true if objects on these layers should collide, false otherwise.
         */
        bool shouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
    };
} // namespace gcep
