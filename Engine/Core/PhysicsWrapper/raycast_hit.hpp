#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Engine/Core/Maths/vector3.hpp"

namespace gcep {

    /**
     * @struct RaycastHit
     * @brief Represents the result of a physics raycast.
     *
     * Contains information about whether a ray hit a physics body,
     * the hit position, surface normal, distance, and the body ID.
     *
     * This struct is usually returned by PhysicsWorld::raycast().
     */
    struct RaycastHit
    {
        friend class PhysicsWorld; ///< PhysicsWorld can access the bodyID.

        /// True if the ray intersected a physics object.
        bool hasHit = false;

        /// World-space position where the ray hit the object.
        Vector3<float> point;

        /// Surface normal at the hit point.
        Vector3<float> normal;

        /// Distance from the ray origin to the hit point.
        float distance = 0.0f;

    private:
        /// Jolt BodyID of the hit physics body.
        JPH::BodyID bodyID;
    };
} // namespace gcep
