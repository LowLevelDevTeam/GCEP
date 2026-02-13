#pragma once

// STL
#include <memory>

// Libs
//Mendatory to be called for any Jolt Header
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyID.h>

//Core
#include "PhysicsManager.hpp"
#include "Layers/BPLayerInterfaceImpl.hpp"
#include "Layers/ObjectLayerPairFilterImpl.hpp"
#include "Layers/ObjectVsBroadPhaseLayerFilterImpl.hpp"

#include "ObjectPhysicsData.hpp"

namespace gcep
{
    class PhysicsBody;

    class PhysicsWorld
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;

        void init();
        void step(float deltaTime);
        void shutdown();

        void createBody(std::shared_ptr<ObjectPhysicsData> data) const;
        void destroyBody(const JPH::BodyID &body_id) const;

    private:
        std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
        std::unique_ptr<JPH::TempAllocator> m_tempAllocator;
        std::unique_ptr<JPH::JobSystem> m_jobSystem;
        std::unique_ptr<BPLayerInterfaceImpl> m_broadPhaseLayerInterface;
        std::unique_ptr<ObjectLayerPairFilterImpl> m_objectLayerPairFilter;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBroadPhaseLayerFilter;

    };
}

//Enum : Square / sphere / cylinder

// PhysicsSystem => createBody() => PhysicsWorld::createBody(), garde une reference au PhysicsWorld (Singleton)

// PhysicsWorld tourne sur un thread a part (fixedUpdate) a 60FPS
// PhysicsWorld=> initJolt(), shutdownJolt(), createBody(Enum shape), physicsTick(), references aux bodies (body interface) (Singleton)

// PhysicsBody => BodyID, reference au world, description physique
// Description physique: Type (static, dynamic, kinematic), position, rotation, physicsShape, mass, friction
// physicsShape => Interface qui pourra etre derivée en plusieurs shapes => BoxShape, SphereShape
