#include "PhysicalWorld.hpp"

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

#include "glm/ext/scalar_common.inl"

using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;

namespace gcep {

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

    	m_broadPhaseLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
    	m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();
    	m_objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();

    	m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();

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

    void PhysicsWorld::step(float deltaTime)
    {
    	m_physicsSystem->Update(deltaTime, 1, m_tempAllocator.get(), m_jobSystem.get());
    }

    void PhysicsWorld::createBody(PhysicsSystem& ps, std::shared_ptr<ObjectPhysicsData> data) const
    {
    	JPH::ShapeRefC shape;
    	Vec3 defaultScale = {1.0f, 1.0f, 1.0f};

    	switch (data->getShapeType()) {
    		case EShapeType::CUBE :
    		{
    			JPH::BoxShapeSettings cubeShapeSettings(JPH::Vec3(100.0f, 100.0f, 100.0f));
    			shape = cubeShapeSettings.Create().Get();
    			if (data->getScale() != defaultScale) {
    				//scaleshaped
    			}
    		}
    			break;
    		case EShapeType::CYLINDER :
    		{
    			JPH::CylinderShapeSettings cylinderShapeSettings(100.0f, 50);
    			shape = cylinderShapeSettings.Create().Get();
    			if (data->getScale() != defaultScale) {
    				//scaleshaped
    			}
    		}
    			break;
    		case EShapeType::SPHERE :
    		{
    			JPH::SphereShapeSettings sphereShapeSettings(50.0f);
    			shape = sphereShapeSettings.Create().Get();
    			if (data->getScale() != defaultScale) {
    				//scaleshaped
    			}
    		}
    			break;
    	}

    	JPH::EMotionType motionType;

    	switch (data->getMotionType()) {
    		case EMotionType::STATIC : {
    			motionType = JPH::EMotionType::Static;
    		}
    			break;
    		case EMotionType::KINEMATIC : {
    			motionType = JPH::EMotionType::Kinematic;
    		}
    			break;
    		case EMotionType::DYNAMIC : {
    			motionType = JPH::EMotionType::Dynamic;
    		}
    			break;
    	}

    	JPH::RVec3 position = JPH::Vec3(data->getPosition()[1], data->getPosition()[2], data->getPosition()[3]);
    	JPH::Quat rotation = JPH::Quat(data->getRotation()[1], data->getRotation()[2], data->getRotation()[3], data->getRotation()[4]);
    	JPH::BodyCreationSettings bodySettings(shape, position, rotation, motionType, static_cast<int>(data->getLayers()));

    	JPH::Body *body = m_physicsSystem->GetBodyInterface().CreateBody(bodySettings);

    	body->SetLinearVelocity(JPH::Vec3(data->getLinearVelocity()[1], data->getLinearVelocity()[2], data->getLinearVelocity()[3]));
    	body->SetAngularVelocity(JPH::Vec3(data->getAngularVelocity()[1], data->getAngularVelocity()[2], data->getAngularVelocity()[3]));

    	m_physicsSystem->GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::Activate);

		ps.m_tempBodyID = body->GetID();
    }

    void PhysicsWorld::destroyBody(const JPH::BodyID &body_id) const
    {
    	m_physicsSystem->GetBodyInterface().RemoveBody(body_id);
    	m_physicsSystem->GetBodyInterface().DestroyBody(body_id);
    }

}
