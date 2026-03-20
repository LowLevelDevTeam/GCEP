#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>
#include <Maths/vector3.hpp>

namespace gcep::ECS
{
    enum class EMotionType
    {
        STATIC,
        DYNAMIC,
        KINEMATIC
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

    struct RigidBodyComponent
    {
        EMotionType motionType = EMotionType::STATIC;
        float mass = 1.0f;
        float linearDamping = 0.0f;
        float angularDamping = 0.05f;
        float gravityFactor = 1.0f;
        static inline bool _gcep_registered =
    ComponentRegistry::instance().reg<RigidBodyComponent>();
    };

    struct BoxColliderComponent {
        Vector3<float> halfExtents { 0.5f, 0.5f, 0.5f };
        Vector3<float> offset      { 0.f,  0.f,  0.f  };
        bool           isTrigger   = false;
        static inline bool _gcep_registered =
        ComponentRegistry::instance().reg<BoxColliderComponent>();
  };

  struct SphereColliderComponent {
        float          radius    = 0.5f;
        Vector3<float> offset    { 0.f, 0.f, 0.f };
        bool           isTrigger = false;
        static inline bool _gcep_registered =
        ComponentRegistry::instance().reg<SphereColliderComponent>();
  };

  struct CapsuleColliderComponent {
        float          radius     = 0.5f;
        float          halfHeight = 0.5f; // hauteur de la partie cylindrique seulement
        Vector3<float> offset     { 0.f, 0.f, 0.f };
        bool           isTrigger  = false;
        static inline bool _gcep_registered =
        ComponentRegistry::instance().reg<CapsuleColliderComponent>();

  };

  struct CylinderColliderComponent {
        float          radius     = 0.5f;
        float          halfHeight = 0.5f;
        Vector3<float> offset     { 0.f, 0.f, 0.f };
        bool           isTrigger  = false;
        static inline bool _gcep_registered =
        ComponentRegistry::instance().reg<CylinderColliderComponent>();
  };


} // namespace gcep::ECS
