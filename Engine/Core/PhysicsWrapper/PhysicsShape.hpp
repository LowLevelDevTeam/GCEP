#pragma once

// Libs
#include <memory>

// Libs (I don't know if we should keep this here)
#include <Jolt/Jolt.h>
#include "Jolt/Core/Reference.h"

// Core
#include "Engine/Core/Maths/Vector3.hpp"

namespace JPH
{
    class Shape;
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

        static std::shared_ptr<PhysicsShape> createBox(const Vector3<float>& halfExtents);
        static std::shared_ptr<PhysicsShape> createSphere(float radius);
        static std::shared_ptr<PhysicsShape> createCylinder(float halfHeight, float radius);
        static std::shared_ptr<PhysicsShape> createCapsule(float halfHeight, float radius);

        static std::shared_ptr<PhysicsShape> createScaled(const std::shared_ptr<PhysicsShape>& baseShape, const Vector3<float>& scale);

        EPhysicsShapeType getType() const;
        JPH::RefConst<JPH::Shape> getJoltShape() const;

    private:
        EPhysicsShapeType m_type;
        JPH::RefConst<JPH::Shape> m_shape;
    };
} // gcep

