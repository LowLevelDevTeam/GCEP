#include "physics_world.hpp"

#include <memory>
#include <iostream>

//LIB

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

// Core
#include "physics_shape.hpp"
#include "Jolt/Physics/Collision/Shape/ScaledShape.h"

namespace gcep
{

    PhysicsWorld::PhysicsWorld()
    {
       // Register allocation hook.
    	JPH::RegisterDefaultAllocator();

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
		JPH::Factory::sInstance = new JPH::Factory();

		// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
		JPH::RegisterTypes();

    	constexpr size_t cTempAllocatorSize = 16 * 1024 * 1024; // 16 Mo
    	m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(cTempAllocatorSize);

    	m_broadPhaseLayerInterface = std::make_unique<BPlayerInterfaceImpl>();
    	m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();
    	m_objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();

    	m_physicsSystem = std::make_shared<JPH::PhysicsSystem>();

		// This is the max amount of rigid bodies that you can add to the physics system.
		constexpr glm::uint cMaxBodies = 65536;

		// This determines how many mutexes to allocate to protect rigid bodies from concurrent access.
		constexpr glm::uint cNumBodyMutexes = 0;

		// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
		// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase).
		constexpr glm::uint cMaxBodyPairs = 65536;

		// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
		// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
		constexpr glm::uint cMaxContactConstraints = 10240;

    	constexpr glm::uint cMaxJobs = 65536;
    	constexpr glm::uint cMaxBarriers = 65536;

    	glm::uint numThreads = std::thread::hardware_concurrency() - 1;

    	m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
			cMaxJobs,
			cMaxBarriers,
			numThreads
		);

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

	PhysicsWorld::~PhysicsWorld()
    {
    	JPH::BodyIDVector allBodies;
    	m_physicsSystem->GetBodies(allBodies);

    	JPH::BodyInterface &bodyInterface = m_physicsSystem->GetBodyInterface();

    	bodyInterface.RemoveBodies(allBodies.data(), allBodies.size());
    	bodyInterface.DestroyBodies(allBodies.data(), allBodies.size());

    	JPH::UnregisterTypes();

    	// Destroy the factory
    	delete JPH::Factory::sInstance;
    	JPH::Factory::sInstance = nullptr;
    }

    void PhysicsWorld::step(float deltaTime) const
    {
    	m_physicsSystem->Update(deltaTime, 1, m_tempAllocator.get(), m_jobSystem.get());
    }


    void PhysicsWorld::createBody(PhysicsComponent& data) const
    {
    	JPH::ShapeRefC baseShape;

    	// Creating unscaled base shape
    	switch (data.shapeType)
    	{
    		case EShapeType::CUBE :
    		{
    			JPH::BoxShapeSettings cubeShapeSettings(JPH::Vec3(100.0f, 100.0f, 100.0f));
    			baseShape = cubeShapeSettings.Create().Get();
    			break;
    		}
    		case EShapeType::CYLINDER :
    		{
    			JPH::CylinderShapeSettings cylinderShapeSettings(100.0f, 50.0f);
    			baseShape = cylinderShapeSettings.Create().Get();
    			break;
    		}
    		case EShapeType::SPHERE :
    		{
    			JPH::SphereShapeSettings sphereShapeSettings(50.0f);
    			baseShape = sphereShapeSettings.Create().Get();
    			break;
    		}
    	}

    	if (!baseShape) return;

    	// Scaling
    	JPH::ShapeRefC finalShape = baseShape;
    	const auto scale = data.scale;
    	const bool hasScale = scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f;

    	if (hasScale)
    	{
    		JPH::ScaledShapeSettings scaledShapeSettings(
    			baseShape,
    			JPH::Vec3(scale.x, scale.y, scale.z)
    			);

    		auto result = scaledShapeSettings.Create();
    		if (result.HasError()) return;

    		finalShape = result.Get();
    	}

    	// Motion type
    	JPH::EMotionType motionType = JPH::EMotionType::Static;
    	switch (data.motionType)
    	{
    		case EMotionType::STATIC :
    		{
    			motionType = JPH::EMotionType::Static;
    			break;
    		}
    		case EMotionType::KINEMATIC :
    		{
    			motionType = JPH::EMotionType::Kinematic;
    			break;
    		}
    		case EMotionType::DYNAMIC :
    		{
    			motionType = JPH::EMotionType::Dynamic;
    			break;
    		}
    	}

    	// Transform
    	const Vector3<float>& pos = data.position;
    	const Quaternion& rot = data.rotation;

    	JPH::RVec3 position(pos.x, pos.y, pos.z);
    	JPH::Quat rotation(rot.x, rot.y, rot.z, rot.w);

    	// Body creation
    	JPH::BodyCreationSettings bodySettings(
    		finalShape,
			position,
			rotation,
			motionType,
			static_cast<int>(data.layers)
    		);

    	JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
    	JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    	bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
    }

    void PhysicsWorld::destroyBody(const JPH::BodyID &body_id) const
    {
    	m_physicsSystem->GetBodyInterface().RemoveBody(body_id);
    	m_physicsSystem->GetBodyInterface().DestroyBody(body_id);
    }

}
