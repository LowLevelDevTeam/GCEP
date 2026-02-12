#pragma once

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/MathTypes.h>

// Core

using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;

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

        void setPosition(const Vec3& position);
        void setRotation(const Quat& rotation);

        [[nodiscard]] JPH::RVec3 GetCenterOfMassPosition() const;
        [[nodiscard]] JPH::AABox GetWorldSpaceBounds() const;

        // g
        void destroy();
    private:
        JPH::BodyID m_bodyId;
        PhysicsWorld* m_world;
    };
} // gcep
