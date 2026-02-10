#pragma once

// Libs
#include <Jolt/Physics/Body/BodyID.h>

namespace gcep
{
    class PhysicsBodyDesc;

    class PhysicsBody
    {
    public:
        PhysicsBody();
        ~PhysicsBody();

    private:
        JPH::BodyID m_bodyId;
        PhysicsBodyDesc m_bodyDesc;
    };
} // gcep
