#pragma once

// STL
#include <memory>
#include <unordered_map>

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>

// Core
#include "Layers/bplayer_interface_impl.hpp"
#include "Layers/object_layer_pair_filter_impl.hpp"
#include "Layers/object_vs_broad_phase_layer_filter_impl.hpp"

#include "Component/physics_component.hpp"
#include "Component/transform_component.hpp"
#include "raycast_hit.hpp"

namespace gcep
{
    class PhysicsBody;

    /**
     * @class PhysicsWorld
     * @brief Low-level wrapper for the physics simulation.
     *
     * PhysicsWorld manages the internal Jolt PhysicsSystem and provides
     * high-level functions for body creation, simulation stepping, and
     * physics queries. It also caches collision shapes to reduce
     * redundant allocations.
     */
    class PhysicsWorld
    {
        friend class PhysicsSystem;

    public:
        /**
         * @brief Constructs a new PhysicsWorld instance.
         *
         * Initializes the physics system, allocators, job system, and collision layers.
         */
        PhysicsWorld();

        /**
         * @brief Destroys the PhysicsWorld instance.
         *
         * Cleans up all physics bodies, unregisters Jolt types, and releases resources.
         */
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete; ///< PhysicsWorld is non-copyable.

        /**
         * @brief Advances the physics simulation by a fixed time step.
         *
         * @param dt Delta time in seconds.
         */
        void step(float dt) const;

        /**
         * @brief Creates a physics body from ECS components and registers it in the simulation.
         *
         * @param transform Reference to the entity's TransformComponent.
         * @param data Reference to the entity's PhysicsComponent.
         * @param dataId Output parameter receiving the Jolt BodyID of the created body.
         */
        void createBody(TransformComponent& transform, PhysicsComponent& data, JPH::BodyID& dataId);

        /**
         * @brief Destroys a physics body.
         *
         * @param body_id The BodyID of the body to remove.
         */
        void destroyBody(const JPH::BodyID &body_id) const;

        /**
         * @brief Performs a raycast in the physics world.
         *
         * @param origin Ray origin in world space.
         * @param direction Normalized ray direction.
         * @param maxDistance Maximum distance to trace.
         * @return RaycastHit containing hit information if an object is hit.
         */
        RaycastHit raycast(const Vector3<float>& origin, const Vector3<float>& direction, float maxDistance) const;

        // ======================
        // Shape caching interface
        // ======================

        /**
         * @brief Retrieves a cached box shape or creates it if not already cached.
         *
         * @param halfExtents Half extents of the box along each axis.
         * @return Shared pointer to a PhysicsShape representing a box.
         */
        std::shared_ptr<PhysicsShape> getOrCreateBox(const Vector3<float>& halfExtents);

        /**
         * @brief Retrieves a cached sphere shape or creates it if not already cached.
         *
         * @param radius Sphere radius.
         * @return Shared pointer to a PhysicsShape representing a sphere.
         */
        std::shared_ptr<PhysicsShape> getOrCreateSphere(float radius);

        /**
         * @brief Retrieves a cached cylinder shape or creates it if not already cached.
         *
         * @param halfHeight Half of the cylinder height.
         * @param radius Cylinder radius.
         * @return Shared pointer to a PhysicsShape representing a cylinder.
         */
        std::shared_ptr<PhysicsShape> getOrCreateCylinder(float halfHeight, float radius);

        /**
         * @brief Retrieves a cached capsule shape or creates it if not already cached.
         *
         * @param halfHeight Half of the capsule height.
         * @param radius Capsule radius.
         * @return Shared pointer to a PhysicsShape representing a capsule.
         */
        std::shared_ptr<PhysicsShape> getOrCreateCapsule(float halfHeight, float radius);

        /**
         * @brief Retrieves a cached scaled shape or creates it if not already cached.
         *
         * @param baseShape The base PhysicsShape to scale.
         * @param scale Scale vector to apply.
         * @return Shared pointer to the scaled PhysicsShape.
         */
        std::shared_ptr<PhysicsShape> getOrCreateScaled(std::shared_ptr<PhysicsShape> baseShape, Vector3<float> scale);

    private:
        std::shared_ptr<JPH::PhysicsSystem> m_physicsSystem;

        std::unique_ptr<JPH::TempAllocator> m_tempAllocator;                 ///< Temporary memory allocator for physics simulation.
        std::unique_ptr<JPH::JobSystem> m_jobSystem;                         ///< Multithreaded job system for physics updates.
        std::unique_ptr<BPlayerInterfaceImpl> m_broadPhaseLayerInterface;   ///< Broad-phase collision layer interface.
        std::unique_ptr<ObjectLayerPairFilterImpl> m_objectLayerPairFilter; ///< Filter for object-object collisions.
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBroadPhaseLayerFilter; ///< Filter for object-broadphase collisions.

        // ======================
        // Shape cache
        // ======================
        struct ShapeKey
        {
            EPhysicsShapeType type = EPhysicsShapeType::BOX; ///< Shape type.
            Vector3<float> scale{};                          ///< Scale applied to the shape.
            float radius = 0.f;                              ///< Radius (for sphere/capsule/cylinder).
            float height = 0.f;                              ///< Height (for capsule/cylinder).

            bool operator==(const ShapeKey& other) const
            {
                return type == other.type &&
                       scale == other.scale &&
                       radius == other.radius &&
                       height == other.height;
            }
        };

        struct ShapeKeyHasher
        {
            size_t operator()(const ShapeKey& key) const
            {
                size_t h1 = std::hash<int>()((int)key.type);
                size_t h2 = std::hash<float>()(key.scale.x);
                size_t h3 = std::hash<float>()(key.scale.y);
                size_t h4 = std::hash<float>()(key.scale.z);
                size_t h5 = std::hash<float>()(key.radius);
                size_t h6 = std::hash<float>()(key.height);

                return h1 ^ h2 ^ h3 ^ h4 ^ h5 ^ h6;
            }
        };

        std::unordered_map<ShapeKey, std::weak_ptr<PhysicsShape>, ShapeKeyHasher> m_shapeCache; ///< Cache for all created shapes.
    };
} // namespace gcep