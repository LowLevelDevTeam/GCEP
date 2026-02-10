#pragma once

// STL
#include <memory>

// Libs
#include <Jolt/Jolt.h>

namespace gcep
{
    class PhysicsShape;

    enum class PhysicsBodyType
    {
        Static,
        Dynamic,
        Kinematic
    };

    struct PhysicsBodyDesc
    {
        PhysicsBodyType type;

        JPH::RVec3Arg position{0.f,0.f,0.f};
        JPH::QuatArg rotation{0.f,0.f,0.f,1.f};

        std::shared_ptr<PhysicsShape> shape;

        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
    };

} // gcep
