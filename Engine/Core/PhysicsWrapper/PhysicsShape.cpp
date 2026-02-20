#include "PhysicsShape.hpp"

// STL
#include <memory>

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/ScaledShape.h"

// Core
#include "Engine/Core/Maths/Utils/Vector3Converter.hpp"

namespace gcep
{
    PhysicsShape::PhysicsShape(EPhysicsShapeType type, JPH::RefConst<JPH::Shape> shape)
        : m_type(type), m_shape(shape)
    {}

    std::shared_ptr<PhysicsShape> PhysicsShape::createBox(const Vector3<float>& halfExtents)
    {
        JPH::BoxShapeSettings settings(Vector3Converter::ToJolt(halfExtents));

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) return nullptr;

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::BOX, result.Get());
        return newShape;
    }

    std::shared_ptr<PhysicsShape> PhysicsShape::createSphere(float radius)
    {
        JPH::SphereShapeSettings settings(radius);

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) return nullptr;

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::SPHERE, result.Get());
        return newShape;
    }

    std::shared_ptr<PhysicsShape> PhysicsShape::createCylinder(float halfHeight, float radius)
    {
        JPH::CylinderShapeSettings settings(halfHeight, radius);

        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) return nullptr;

        auto newShape = std::make_shared<PhysicsShape>(EPhysicsShapeType::CAPSULE, result.Get());
        return newShape;
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
        return newShape;
    }

    std::shared_ptr<PhysicsShape> PhysicsShape::createScaled(const std::shared_ptr<PhysicsShape> &baseShape, const Vector3<float> &scale)
    {
        JPH::ScaledShapeSettings settings(
            baseShape->getJoltShape(),
            Vector3Converter::ToJolt(scale)
        );

        auto result = settings.Create();
        if (result.HasError()) return nullptr;

        auto scaledShape = std::make_shared<PhysicsShape>(baseShape->getType(), result.Get());
        return scaledShape;
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