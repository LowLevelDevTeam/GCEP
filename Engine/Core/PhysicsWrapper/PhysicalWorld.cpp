#include "PhysicalWorld.hpp"

#include <memory>
#include <iostream>

//LIB

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace gcep {

    PhysicsWorld::PhysicsWorld()
    {
       initWorld();
    }

    PhysicsWorld::~PhysicsWorld()
    {
        shutdownWorld();
    }

    void PhysicsWorld::initWorld()
    {
    	// Register allocation hook.
    	JPH::RegisterDefaultAllocator();

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
		JPH::Factory::sInstance = new JPH::Factory();

		// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
		JPH::RegisterTypes();

    	m_broadPhaseLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
    	m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();
    	m_objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();

    	m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();

		// This is the max amount of rigid bodies that you can add to the physics system.
		const glm::uint cMaxBodies = 65536;

		// This determines how many mutexes to allocate to protect rigid bodies from concurrent access.
		const glm::uint cNumBodyMutexes = 0;

		// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
		// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase).
		const glm::uint cMaxBodyPairs = 65536;

		// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
		// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
		const glm::uint cMaxContactConstraints = 10240;

		m_physicsSystem->Init(
			cMaxBodies,
			cNumBodyMutexes,
			cMaxBodyPairs,
			cMaxContactConstraints,
			*m_broadPhaseLayerInterface,
			*m_objectVsBroadPhaseLayerFilter,
			*m_objectLayerPairFilter
			);
    }

    void PhysicsWorld::step(float deltaTime)
    {

    }

    void PhysicsWorld::shutdownWorld()
    {
    	JPH::BodyIDVector allBodies;
    	m_physicsSystem->GetBodies(allBodies);

    	JPH::BodyInterface &bodyInterface = m_physicsSystem->GetBodyInterface();

    	bodyInterface.RemoveBodies(allBodies.data(), allBodies.size());
    	bodyInterface.DestroyBodies(allBodies.data(), allBodies.size());

    }

    void PhysicsWorld::createBody(const EShapeType shapeType) const
    {
    	JPH::ShapeRefC shape;

    	switch (shapeType) {
    			case EShapeType::CUBE : {
    				JPH::BoxShapeSettings cubeShapeSettings(JPH::Vec3(100.0f, 100.0f, 100.0f));
    				shape = cubeShapeSettings.Create().Get();
    		    }

    			case EShapeType::CYLINDER : {
    				JPH::CylinderShapeSettings cylinderShapeSettings(100.0f, 50);
    				shape = cylinderShapeSettings.Create().Get();
    		    }

    			case EShapeType::SPHERE : {
    				JPH::SphereShapeSettings sphereShapeSettings(50.0f);
    				shape = sphereShapeSettings.Create().Get();
    		    }
    	}
    	JPH::RVec3 position = JPH::RVec3::sZero();
    	JPH::Quat rotation = JPH::Quat::sIdentity();
    	JPH::BodyCreationSettings bodySettings(shape, position, rotation, JPH::EMotionType::Static, Layers::NON_MOVING);

    	JPH::Body *body = m_physicsSystem->GetBodyInterface().CreateBody(bodySettings);
    	m_physicsSystem->GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::Activate);
    }

    void PhysicsWorld::destroyBody(const JPH::BodyID &body_id) const
    {
    	m_physicsSystem->GetBodyInterface().RemoveBody(body_id);
    	m_physicsSystem->GetBodyInterface().DestroyBody(body_id);
    }

}
