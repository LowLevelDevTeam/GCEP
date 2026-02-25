#pragma once

// STL
#include <memory>

// Libs
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>

//Core
#include "Layers/bplayer_interface_impl.hpp"
#include "Layers/object_layer_pair_filter_impl.hpp"
#include "Layers/object_vs_broad_phase_layer_filter_impl.hpp"

#include "Component/physics_component.hpp"

namespace gcep
{
    class PhysicsBody;

    class PhysicsWorld
    {
        friend class PhysicsSystem;

    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;

        void step(float dt) const;
        void createBody(PhysicsComponent& data, JPH::BodyID& dataId);
        void destroyBody(const JPH::BodyID &body_id) const;

    private:
        std::shared_ptr<JPH::PhysicsSystem> m_physicsSystem;
        std::unique_ptr<JPH::TempAllocator> m_tempAllocator;
        std::unique_ptr<JPH::JobSystem> m_jobSystem;
        std::unique_ptr<BPlayerInterfaceImpl> m_broadPhaseLayerInterface;
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
