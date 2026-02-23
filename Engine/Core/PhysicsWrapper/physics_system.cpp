#include "physics_system.hpp"

#include <memory>

#include "physics_world.hpp"

namespace gcep
{
    PhysicsSystem::PhysicsSystem()
    {
        init();
    }

    PhysicsSystem::~PhysicsSystem()
    {
        shutdown();
    }

    PhysicsSystem* PhysicsSystem::getInstance()
    {

        if (!s_instance)
        {
            s_instance = new PhysicsSystem();
        }

        return s_instance;
    }


    void PhysicsSystem::init()
    {
        m_world = std::make_unique<PhysicsWorld>();
    }

    void PhysicsSystem::shutdown()
    {
        m_world.reset();
    }

    void PhysicsSystem::startSimulation()
    {
        //Récupère tous les PhysicsComponent
        //auto PC : allPhysicsComponent :
        //m_world.createBody(PC, PC.bodyID);
        //PC.m_physicsSystem = m_world.m_physicsSystem
    }

    void PhysicsSystem::update(float dt)
    {
        m_world->step(dt);
    }

    void PhysicsSystem::stopSimulation()
    {
        //Récupère tous les PhysicsComponent
        //auto PC : allPhysicsComponent :
        //m_world.destroyBody(PC.BodyID);
    }

    PhysicsSystem* PhysicsSystem::s_instance = nullptr;
} // gcep