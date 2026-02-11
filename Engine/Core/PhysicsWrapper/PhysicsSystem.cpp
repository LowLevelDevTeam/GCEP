//
// Created by momop on 10/02/2026.
//

#include "PhysicsSystem.hpp"
#include "PhysicalWorld.hpp"

namespace gcep
{
    PhysicsSystem::PhysicsSystem()
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