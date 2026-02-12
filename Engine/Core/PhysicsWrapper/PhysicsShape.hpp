#pragma once

// Libs
#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace gcep
{
    class PhysicsShape
    {
    public:
        virtual ~PhysicsShape() = default;

        JPH::Shape* getNativeShape() const;

    protected:
        PhysicsShape();

        std::shared_ptr<JPH::Shape> m_shape;
    };

    class BoxShape final : public PhysicsShape
    {
    public:
        BoxShape(float width, float height, float depth);
    };

    class SphereShape final : public PhysicsShape
    {
    public:
        explicit SphereShape(float radius);
    };
} // gcep

