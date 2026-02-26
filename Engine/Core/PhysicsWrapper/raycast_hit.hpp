#pragma once


#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Engine/Core/Maths/vector3.hpp"

namespace gcep {

    struct RaycastHit
    {
        friend class PhysicsWorld;

        bool hasHit = false;
        Vector3<float> point;
        Vector3<float> normal;
        float distance = 0.0f;

        // private:
        JPH::BodyID bodyID;
    };
} // gcep
