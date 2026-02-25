#include "physics_system.hpp"

#include <memory>

#include "physics_world.hpp"
#include "Engine/Core/Entity-Component-System/headers/registry.hpp"

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

    PhysicsSystem& PhysicsSystem::getInstance()
    {

        static PhysicsSystem s_instance;
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
        Registry reg;

        auto view = reg.partialView<PhysicsComponent>();

        for (EntityID id : view)
        {
            auto& pc = view.get<PhysicsComponent>(id); // getter
            m_world->createBody(pc, pc.m_bodyIDRef);
        }
    }

    void PhysicsSystem::update(float dt)
    {
        m_world->step(dt);
        // need sync transform & body
    }

    void PhysicsSystem::stopSimulation()
    {
        Registry reg;

        auto view = reg.partialView<PhysicsComponent>();

        for (EntityID id : view) {
            auto& pc = view.get<PhysicsComponent>(id); // getter
            m_world->destroyBody(pc.m_bodyIDRef);
        }
    }

} // gcep