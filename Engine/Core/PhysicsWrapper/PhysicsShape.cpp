#include "PhysicsShape.hpp"

#include <memory>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

namespace gcep
{
    PhysicsShape::PhysicsShape(EPhysicsShapeType type, JPH::RefConst<JPH::Shape> shape)
        : m_type(type), m_shape(shape)
    {}

    std::shared_ptr<PhysicsShape> PhysicsShape::createBox(const JPH::Vec3& halfExtents)
    {
        JPH::BoxShapeSettings settings(halfExtents);

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError())
        {
            return nullptr;
        }

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::BOX, result.Get());
    }

    std::shared_ptr<PhysicsShape> PhysicsShape::createSphere(float radius)
    {
        JPH::SphereShapeSettings settings(radius);

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError())
        {
            return nullptr;
        }

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::SPHERE, result.Get());
    }

    std::shared_ptr<PhysicsShape> PhysicsShape::createCylinder(float halfHeight, float radius)
    {
        JPH::CylinderShapeSettings settings(halfHeight, radius);

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError())
        {
            return nullptr;
        }

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::CAPSULE, result.Get());
    }

    std::shared_ptr<PhysicsShape> PhysicsShape::createCapsule(float halfHeight, float radius)
    {
        JPH::CapsuleShapeSettings settings(halfHeight, radius);

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError())
        {
            return nullptr;
        }

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::CAPSULE, result.Get());
    }

    EPhysicsShapeType PhysicsShape::getType() const
    {
        return m_type;
    }

    JPH::RefConst<JPH::Shape> PhysicsShape::getJoltShape() const
    {
        return m_shape;
    }
} // gcep