#pragma once

// STL
#include <memory>

// Libs
#include <Jolt/Jolt.h>

namespace gcep
{
    enum class EShapeType
    {
        CUBE,
        SPHERE,
        CYLINDER
    };

    struct PhysicsBodyDesc
    {
        EShapeType shape;

        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Quat rotation = JPH::Quat::sIdentity();
    };

} // gcep
