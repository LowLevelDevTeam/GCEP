#pragma once
#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Engine/Core/Maths/vector3.hpp"
#include "Engine/Core/PhysicsWrapper/physics_shape.hpp"

namespace gcep
{
    /**
     * @enum EShapeType
     * @brief Represents the type of collision shape used by a physics body.
     */
    enum class EShapeType
    {
        CUBE,    ///< Box-shaped collision.
        SPHERE,  ///< Sphere-shaped collision.
        CYLINDER,///< Cylinder-shaped collision.
        CAPSULE  ///< Capsule-shaped collision.
    };

    /**
     * @enum EMotionType
     * @brief Describes the motion behavior of a physics body.
     */
    enum class EMotionType
    {
        STATIC,    ///< The body does not move and is unaffected by forces.
        DYNAMIC,   ///< The body moves and reacts to physics forces.
        KINEMATIC  ///< The body moves but is controlled manually, unaffected by physics forces.
    };

    /**
     * @enum ELayers
     * @brief Collision layer of a physics body.
     */
    enum class ELayers
    {
        NON_MOVING = 0, ///< Static or environment objects.
        MOVING = 1      ///< Dynamic or player-controlled objects.
    };

    /**
     * @struct PhysicsComponent
     * @brief Component representing the physical properties of an entity.
     *
     * Contains all necessary data for a physics simulation, including velocity,
     * collision shape type, motion type, and collision layers.
     * Typically used with the PhysicsSystem to create and update physics bodies.
     */
    struct PhysicsComponent
    {
        friend class PhysicsSystem; ///< PhysicsSystem can access private members.

        /// Linear velocity of the physics body.
        Vector3<float> linearVelocity = Vector3<float>(0.f, 0.f, 0.f);

        /// Angular velocity of the physics body.
        Vector3<float> angularVelocity = Vector3<float>(0.f, 0.f, 0.f);

        // ======================
        // Physics properties (editable in IMGUI)
        // ======================

        /// Type of the collision shape.
        EShapeType shapeType = EShapeType::CUBE;

        /// Motion type of the physics body.
        EMotionType motionType = EMotionType::STATIC;

        /// Collision layer of the physics body.
        ELayers layers = ELayers::NON_MOVING;

    private:
        /// Reference to the internal Jolt BodyID for this entity.
        JPH::BodyID m_bodyIDRef;
    };
} // namespace gcep