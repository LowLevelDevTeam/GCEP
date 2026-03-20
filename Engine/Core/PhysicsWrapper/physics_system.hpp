#pragma once

// Internals
#include <ECS/Components/physics_component.hpp>
#include <ECS/headers/registry.hpp>
#include <PhysicsWrapper/physics_world.hpp>
#include <PhysicsWrapper/raycast_hit.hpp>

// STL
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gcep
{
    class PhysicsWorld;

    struct ScriptCollisionEvent
    {
        ECS::EntityID self;
        ECS::EntityID other;
        float contactX, contactY, contactZ;
        float normalX,  normalY,  normalZ;
        bool  isTrigger;
    };

    /**
     * @class PhysicsSystem
     * @brief Singleton wrapper for the engine's physics subsystem.
     *
     * PhysicsSystem manages the physics simulation,
     * updating the physics world, syncing physics results to entity transforms,
     * and performing queries like raycasts.
     *
     * It internally owns a PhysicsWorld instance and interacts with the ECS Registry
     * to manage PhysicsComponents.
     */
    class PhysicsSystem
    {
    public:
        /**
         * @brief Constructs a new PhysicsSystem instance.
         *
         * Initializes internal members. For proper usage, prefer using getInstance().
         */
        PhysicsSystem();

        /**
         * @brief Destroys the PhysicsSystem instance.
         *
         * Cleans up the PhysicsWorld and releases resources.
         */
        ~PhysicsSystem();

        /**
         * @brief Deleted copy constructor.
         *
         * PhysicsSystem is a singleton; copying is not allowed.
         */
        PhysicsSystem(const PhysicsSystem&) = delete;

        /**
         * @brief Accesses the singleton instance of PhysicsSystem.
         *
         * @return Reference to the PhysicsSystem singleton.
         */
        static PhysicsSystem& getInstance();

        /**
         * @brief Initializes the physics subsystem.
         *
         * Allocates the PhysicsWorld.
         */
        void init();

        /**
         * @brief Shuts down the physics subsystem.
         *
         * Cleans up the PhysicsWorld and releases all associated resources.
         */
        void shutdown();

        /**
         * @brief Starts the physics simulation for all registered entities.
         *
         * Typically called before the main simulation loop to create physics bodies
         * for entities with PhysicsComponents.
         */
        void startSimulation();

        /**
         * @brief Advances the physics simulation by the given delta time.
         *
         * @param dt Time step to advance the simulation (in seconds).
         */
        void update(float dt);

        /**
         * @brief Stops the physics simulation and removes physics bodies.
         *
         * Typically called when shutting down or resetting the simulation.
         */
        void stopSimulation();

        void setRegistry(ECS::Registry* reg);

        /**
         * @brief Synchronizes entity transforms with the current physics simulation.
         *
         * Copies updated positions and rotations from the physics bodies
         * into the ECS Registry transforms.
         *
         * @param reg Reference to the entity Registry containing PhysicsComponents and ECS::Transforms.
         */
        void syncPhysicsToTransforms(ECS::Registry& reg);

        /**
         * @brief Synchronizes physics bodies with the entity transforms.
         *
         * Updates physics bodies to match the ECS transforms, useful for
         * kinematic objects or editor manipulation.
         *
         * @param reg Reference to the entity Registry containing PhysicsComponents and ECS::Transforms.
         */
        void syncTransformsToPhysics(ECS::Registry& reg);

        void syncComponentsVelocitiesToPhysics(ECS::Registry& reg);
        void syncPhysicsVelocitiesToComponents(ECS::Registry& reg);
        void applyForces(ECS::Registry& reg);

        /**
         * @brief Performs a raycast query in the physics world.
         *
         * @param origin Start point of the ray in world space.
         * @param direction Normalized direction vector of the ray.
         * @param maxDistance Maximum distance to check along the ray.
         * @return RaycastHit structure containing hit information.
         */
        RaycastHit raycast(const Vector3<float>& origin, const Vector3<float>& direction, float maxDistance);

        /// @brief Collision/trigger enter events produced by the last physics step.
        const std::vector<ScriptCollisionEvent>& getEnterEvents() const { return m_enterEvents; }

        /// @brief Collision/trigger exit events produced by the last physics step.
        const std::vector<ScriptCollisionEvent>& getExitEvents()  const { return m_exitEvents; }

        //void addImpulse(const JPH::BodyID* bodyID, const Vector3<float> impulse) const;
        //void addForce(const JPH::BodyID* bodyID, const Vector3<float> force) const;

        /**
         * @brief Entity-Component-System registry.
         *
         * Holds all entities, including those with PhysicsComponents and ECS::Transforms.
         * Typically this would be private, but exposed here for testing.
         */
        ECS::Registry* registry;

    private:
        std::unique_ptr<PhysicsWorld> m_world; ///< Internal physics world instance.

        std::unordered_map<uint32_t, ECS::EntityID> m_bodyToEntity;
        std::unordered_set<uint32_t>                m_triggerBodies;
        std::vector<ScriptCollisionEvent>           m_enterEvents;
        std::vector<ScriptCollisionEvent>           m_exitEvents;
    };
} // namespace gcep
