#include "PhysicsSystem.hpp"

#include <memory>

#include "PhysicalWorld.hpp"

namespace gcep
{
    PhysicsSystem::PhysicsSystem()
        : m_world(std::make_unique<PhysicsWorld>())
    {
        m_world->initWorld();
    }

    PhysicsSystem::~PhysicsSystem()
    {
        m_world->shutdownWorld();
    }

    void PhysicsSystem::createBody(const PhysicsBodyDesc &desc) const
    {
        m_world->createBody(desc);
    }
} // gcep