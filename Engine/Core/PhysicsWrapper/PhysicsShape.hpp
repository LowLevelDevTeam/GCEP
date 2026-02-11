#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace gcep
{
    class PhysicsShape
    {
    public:
        virtual ~PhysicsShape() = default;

        virtual JPH::Shape* createJoltShape() const = 0;
    };
} // gcep

