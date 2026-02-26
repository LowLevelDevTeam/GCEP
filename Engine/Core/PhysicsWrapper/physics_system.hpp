#pragma once

#include <memory>

#include "physics_world.hpp"

#include "Engine/Core/Entity-Component-System/headers/registry.hpp"

#include "raycast_hit.hpp"


namespace gcep
{
    class PhysicsWorld;

    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&) = delete;

        static PhysicsSystem& getInstance();

        void init();
        void shutdown();

        void startSimulation();
        void update(float dt);
        void stopSimulation();

        void syncPhysicsToTransforms(Registry& reg);
        void syncTransformsToPhysics(Registry& reg);

        RaycastHit raycast(const Vector3<float>& origin, const Vector3<float>& direction, float maxDistance);

        Registry reg; //private

    private:
        std::unique_ptr<PhysicsWorld> m_world;
    };
} // gcep