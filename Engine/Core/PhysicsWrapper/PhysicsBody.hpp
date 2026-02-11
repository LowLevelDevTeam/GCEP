#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Geometry/AABox.h>
#include "Jolt/Math/MathTypes.h"

// Core
#include "PhysicsBodyDesc.hpp"

namespace gcep
{
    class PhysicsBody
    {
    public:
        PhysicsBody(const PhysicsBodyDesc& bodyDesc);
        ~PhysicsBody();

        PhysicsBody(const PhysicsBody&) = delete;
        PhysicsBody& operator=(const PhysicsBody&) = delete;

        PhysicsBody(PhysicsBody&& other) noexcept;
        PhysicsBody& operator=(PhysicsBody&& other) noexcept;

        void AddForce(const JPH::Vec3& force);
        void AddForce(const JPH::Vec3& force, const JPH::RVec3& position);
        void AddTorque(const JPH::Vec3& torque);
        void AddImpulse(const JPH::Vec3& impulse);
        void AddImpulse(const JPH::Vec3& impulse, const JPH::RVec3& position);

        [[nodiscard]] JPH::RVec3 GetCenterOfMassPosition() const;
        [[nodiscard]] JPH::AABox GetWorldSpaceBounds() const;

        [[nodiscard]] JPH::BodyID GetBodyId() const;

    private:
        void destroy();

    private:
        JPH::BodyID m_bodyId;
        bool m_isValid = false;
    };
} // gcep
