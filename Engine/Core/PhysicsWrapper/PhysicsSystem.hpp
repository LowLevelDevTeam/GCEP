#pragma once

#include <memory>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "PhysicsWorld.hpp"

namespace gcep
{
    class PhysicsWorld;

    class PhysicsSystem
    {
        friend class PhysicsWorld;
        friend class PhysicsComponent;

    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&) = delete;

        static PhysicsSystem* getInstance();

        void init();
        void shutdown();

        void startSimulation();
        void update(float dt);
        void stopSimulation();
        //RaycastHit
        //OverlapQuerry

    private:
        std::unique_ptr<PhysicsWorld> m_world;

        static PhysicsSystem* s_instance;
    };
} // gcep