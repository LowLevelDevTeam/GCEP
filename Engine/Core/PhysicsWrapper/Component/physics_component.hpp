#pragma once
#include <memory>
#include "Jolt/Jolt.h"
#include <Jolt/Physics/Body/BodyID.h>

#include "Engine/Core/Maths/vector3.hpp"
#include "Engine/Core/PhysicsWrapper/physics_shape.hpp"

namespace gcep
{
    enum class EShapeType
    {
        CUBE,
        SPHERE,
        CYLINDER
    };

    enum class EMotionType
    {
        STATIC,
        DYNAMIC,
        KINEMATIC
    };

    enum class ELayers
    {
        NON_MOVING = 0,
        MOVING = 1
    };

    struct PhysicsComponent
    {
        friend class PhysicsSystem;

        Vector3<float> linearVelocity = Vector3<float>(0.f,0.f,0.f);
        Vector3<float> angularVelocity = Vector3<float>(0.f,0.f,0.f);

        // To put in IMGUI because we don't want the player to touch to these 3 while simulating

        EShapeType shapeType = EShapeType::CUBE;
        EMotionType motionType = EMotionType::STATIC;
        ELayers layers = ELayers::NON_MOVING;

        JPH::BodyID	m_bodyIDRef; //private
    };
} // gcep
