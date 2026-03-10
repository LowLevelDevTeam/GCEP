#pragma once

// Libs
#include <memory>

// External
#include <Jolt/Jolt.h>
#include "Jolt/Core/Reference.h"

// Core
#include "Engine/Core/Maths/vector3.hpp"

namespace JPH
{
    class Shape;
}

namespace gcep
{
    /**
     * @enum EPhysicsShapeType
     * @brief Enumerates all supported physics collision shape types.
     *
     * This enum describes the high-level abstraction of collision shapes
     * used by the engine. Each type maps internally to a specific Jolt
     * Physics shape implementation.
     */
    enum class EPhysicsShapeType
    {
        BOX,
        SPHERE,
        CAPSULE,
        CYLINDER,
        MESH
    };

    /**
     * @class PhysicsShape
     * @brief High-level wrapper around a Jolt Physics shape.
     *
     * PhysicsShape acts as an abstraction layer over Jolt's internal
     * JPH::Shape representation. It allows the engine to remain decoupled
     * from the physics backend while exposing a clean and controlled API.
     *
     * Instances of this class are typically shared across multiple physics
     * bodies through std::shared_ptr, and are commonly cached to avoid
     * recreating identical shapes.
     */
    class PhysicsShape
    {
    public:

        /**
         * @brief Constructs a PhysicsShape wrapper.
         *
         * @param type The engine-level shape type.
         * @param shape A reference-counted Jolt shape instance.
         */
        PhysicsShape(EPhysicsShapeType type, JPH::RefConst<JPH::Shape> shape);

        /**
         * @brief Creates a box collision shape.
         *
         * @param halfExtents Half extents of the box along each axis.
         * @return A shared pointer to the created PhysicsShape, or nullptr on failure.
         */
        static std::shared_ptr<PhysicsShape> createBox(const Vector3<float>& halfExtents);

        /**
         * @brief Creates a sphere collision shape.
         *
         * @param radius Radius of the sphere.
         * @return A shared pointer to the created PhysicsShape, or nullptr on failure.
         */
        static std::shared_ptr<PhysicsShape> createSphere(float radius);

        /**
         * @brief Creates a cylinder collision shape.
         *
         * @param halfHeight Half of the cylinder height.
         * @param radius Radius of the cylinder.
         * @return A shared pointer to the created PhysicsShape, or nullptr on failure.
         */
        static std::shared_ptr<PhysicsShape> createCylinder(float halfHeight, float radius);

        /**
         * @brief Creates a capsule collision shape.
         *
         * @param halfHeight Half of the capsule height.
         * @param radius Radius of the capsule.
         * @return A shared pointer to the created PhysicsShape, or nullptr on failure.
         */
        static std::shared_ptr<PhysicsShape> createCapsule(float halfHeight, float radius);

        /**
         * @brief Creates a scaled version of an existing shape.
         *
         * This method wraps the base shape into a scaled shape using
         * Jolt's internal scaling mechanism.
         *
         * @param baseShape The base shape to scale.
         * @param scale The scaling factor applied along each axis.
         * @return A shared pointer to the scaled PhysicsShape, or nullptr on failure.
         */
        static std::shared_ptr<PhysicsShape> createScaled(
            const std::shared_ptr<PhysicsShape>& baseShape,
            const Vector3<float>& scale
        );

        /**
         * @brief Returns the high-level engine shape type.
         *
         * @return The EPhysicsShapeType associated with this shape.
         */
        EPhysicsShapeType getType() const;

        /**
         * @brief Returns the underlying Jolt shape.
         *
         * This function provides access to the internal Jolt
         * representation used by the physics engine.
         *
         * @return A reference-counted JPH::Shape.
         */
        JPH::RefConst<JPH::Shape> getJoltShape() const;

    private:
        EPhysicsShapeType m_type;               ///< Engine-level shape type.
        JPH::RefConst<JPH::Shape> m_shape;      ///< Underlying Jolt shape instance.
    };

} // namespace gcep
