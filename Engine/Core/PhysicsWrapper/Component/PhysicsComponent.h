#pragma once

#include "Jolt/Jolt.h"
#include <Jolt/Physics/Body/BodyID.h>

#include "Engine/Core/PhysicsWrapper/PhysicsShape.hpp"

namespace gcep
{
    class PhysicsComponent
    {
    public:


    private :
        JPH::BodyID	m_bodyIDRef;
        PhysicsShape* m_physicsShape = nullptr;
    };
} // gcep
