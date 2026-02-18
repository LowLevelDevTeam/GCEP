#pragma once

#include <memory>

namespace gcep
{
    class PhysicsWorld;

    class PhysicsSystem //=> TODO: Make this a Singleton
    {
        friend class PhysicsWorld;

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
        JPH::BodyID m_tempBodyID;

    };
} // gcep