#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Geometry/AABox.h>

// Core
#include "Engine/Core/Maths/vector3.hpp"
#include "Engine/Core/Maths/quaternion.hpp"

namespace gcep
{
    class PhysicsWorld;

    class PhysicsBody
    {
    public:
        PhysicsBody(PhysicsWorld* world);
        ~PhysicsBody();

        PhysicsBody(const PhysicsBody&) = delete;
        PhysicsBody& operator=(const PhysicsBody&) = delete;

        PhysicsBody(PhysicsBody&& other) noexcept;
        PhysicsBody& operator=(PhysicsBody&& other) noexcept;

        void setPosition(const Vector3<float>& position);
        void setRotation(const Quaternion& rotation);

        [[nodiscard]] Vector3<float> GetCenterOfMassPosition() const;
        [[nodiscard]] JPH::AABox GetWorldSpaceBounds() const;

        // g
        void destroy();
    private:
        JPH::BodyID m_bodyId;
        PhysicsWorld* m_world;
    };
} // gcep
