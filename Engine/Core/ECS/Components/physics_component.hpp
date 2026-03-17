#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>
#include <Maths/vector3.hpp>

namespace gcep::ECS
{
    enum class EShapeType
    {
        CUBE,
        SPHERE,
        CYLINDER,
        CAPSULE
    };

    enum class EMotionType
    {
        STATIC,
        DYNAMIC,
        KINEMATIC
    };

    struct PhysicsComponent
    {
        bool isTrigger = false;

        EShapeType  shapeType  = EShapeType::CUBE;
        EMotionType motionType = EMotionType::STATIC;

        static inline bool _gcep_registered =
            ComponentRegistry::instance().reg<PhysicsComponent>();
    };

    struct PhysicsRuntimeData
    {
        uint32_t       bodyID         = 0xffffffff;
        Vector3<float> linearVelocity  { 0.f, 0.f, 0.f };
        Vector3<float> angularVelocity { 0.f, 0.f, 0.f };
        Vector3<float> force           { 0.f, 0.f, 0.f };
        Vector3<float> impulse         { 0.f, 0.f, 0.f };
        Vector3<float> torque          { 0.f, 0.f, 0.f };
        Vector3<float> angularImpulse  { 0.f, 0.f, 0.f };

        static inline bool _gcep_registered = false;
    };
} // namespace gcep::ECS
