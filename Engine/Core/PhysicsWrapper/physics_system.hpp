#pragma once

#include <memory>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "physics_world.hpp"

#include "Engine/Core/Entity-Component-System/headers/registry.hpp"


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
        //RaycastHit
        //OverlapQuerry

    private:
        std::unique_ptr<PhysicsWorld> m_world;

        Registry reg;

    };
} // gcep