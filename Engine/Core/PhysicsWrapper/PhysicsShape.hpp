#pragma once

// Libs
#include <memory>

#include "Jolt/Jolt.h"
#include "Jolt/Core/Reference.h"

namespace JPH
{
    class Shape;
    class Vec3;
}

namespace gcep
{
    enum class EPhysicsShapeType
    {
        BOX,
        SPHERE,
        CAPSULE,
        MESH
    };

    class PhysicsShape
    {
    public:
        PhysicsShape(EPhysicsShapeType type, JPH::RefConst<JPH::Shape> shape);

        static std::shared_ptr<PhysicsShape> createBox(const JPH::Vec3& halfExtents);
        static std::shared_ptr<PhysicsShape> createSphere(float radius);
        static std::shared_ptr<PhysicsShape> createCylinder(float halfHeight, float radius);
        static std::shared_ptr<PhysicsShape> createCapsule(float halfHeight, float radius);

        EPhysicsShapeType getType() const;
        JPH::RefConst<JPH::Shape> getJoltShape() const;

    private:
        EPhysicsShapeType m_type;
        JPH::RefConst<JPH::Shape> m_shape;

    };
} // gcep

