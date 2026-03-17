#include "physics_world.hpp"

// Internals
#include "physics_shape.hpp"
#include <ECS/Components/physics_component.hpp>
#include <Engine/Core/ECS/Components/transform.hpp>
#include <Maths/Utils/vector3_convertor.hpp>

// Externals
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

// STL
#include <memory>
#include <iostream>

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

    	constexpr size_t cTempAllocatorSize = 128 * 1024 * 1024; // 128 Mo
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

    	constexpr glm::uint cMaxJobs = 4096;
    	constexpr glm::uint cMaxBarriers = 16;

    	glm::uint numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);

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
    	m_contactListener = std::make_unique<PhysicsContactListener>();
		m_physicsSystem->SetContactListener(m_contactListener.get());
    	m_physicsSystem->SetGravity(JPH::Vec3(0, 0, -9.81f));
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

    	m_shapeCache.clear();
    }

    void PhysicsWorld::step(float deltaTime) const
    {
    	m_physicsSystem->Update(deltaTime, 1, m_tempAllocator.get(), m_jobSystem.get());
    }


	void PhysicsWorld::createBody(ECS::Transform& transform, ECS::PhysicsComponent& data, JPH::BodyID& dataId)
    {
    	std::shared_ptr<PhysicsShape> shape;

    	// Shape
    	switch (data.shapeType)
    	{
    		case ECS::EShapeType::CUBE:
    			shape = getOrCreateBox({1.f, 1.f, 1.f});
    			break;
    		case ECS::EShapeType::CYLINDER:
    			shape = getOrCreateCylinder(0.5f, 0.5f);
    			break;
    		case ECS::EShapeType::SPHERE:
    			shape = getOrCreateSphere(0.5f);
    			break;
    		case ECS::EShapeType::CAPSULE:
    			shape = getOrCreateCapsule(0.5f, 0.5f);
    			break;
    		default:
    			shape = getOrCreateBox({1.f, 1.f, 1.f});
    			break;
    	}

    	bool isScaled = transform.scale != Vector3<float>(1.f,1.f,1.f);
    	if (isScaled)
    	{
    		shape = getOrCreateScaled(shape, transform.scale);
    	}

    	if (!shape) return;

    	// Motion type
    	JPH::EMotionType motionType = JPH::EMotionType::Static;
    	switch (data.motionType)
    	{
    		case ECS::EMotionType::STATIC:
    		{
    			motionType = JPH::EMotionType::Static;
    			break;
    		}
    		case ECS::EMotionType::KINEMATIC:
    		{
    			motionType = JPH::EMotionType::Kinematic;
    			break;
    		}
    		case ECS::EMotionType::DYNAMIC:
    		{
    			motionType = JPH::EMotionType::Dynamic;
    			break;
    		}
    	}

    	// Transform
    	const auto& pos = transform.position;
    	const auto& rot = transform.rotation;

    	JPH::RVec3 position(pos.x, pos.y, pos.z);
    	JPH::Quat rotation(rot.x, rot.y, rot.z, rot.w);

    	// Body creation
    	JPH::BodyCreationSettings bodySettings(
    		shape->getJoltShape(),
			position,
			rotation,
			motionType,
			static_cast<JPH::ObjectLayer>(data.layers)
    	);
    	bodySettings.mIsSensor = data.isTrigger;

    	JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
	    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    	/*if (data.motionType != ECS::EMotionType::STATIC)
    	{
    		body->SetAngularVelocity(Vector3Convertor::ToJolt(data.angularVelocity));
    		body->SetLinearVelocity(Vector3Convertor::ToJolt(data.linearVelocity));
    	}*/
	    JPH::EActivation activation = (motionType == JPH::EMotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
	    bodyInterface.AddBody(body->GetID(), activation);
	    dataId = body->GetID();
    }

    void PhysicsWorld::destroyBody(const JPH::BodyID &body_id) const
    {
    	m_physicsSystem->GetBodyInterface().RemoveBody(body_id);
    	m_physicsSystem->GetBodyInterface().DestroyBody(body_id);
    }

	RaycastHit PhysicsWorld::raycast(const Vector3<float>& origin,const Vector3<float>& direction,float maxDistance) const
    {
    	RaycastHit hitResult;

    	JPH::Vec3 dir = JPH::Vec3(direction.x, direction.y, direction.z).Normalized();
    	JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), dir * maxDistance);

    	JPH::RayCastResult result;

    	if (m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, result))
    	{
    		hitResult.hasHit = true;

    		const JPH::BodyLockRead lock(
				m_physicsSystem->GetBodyLockInterface(),
				result.mBodyID
			);

    		if (lock.Succeeded())
    		{
    			const JPH::Body& body = lock.GetBody();

    			JPH::RVec3 worldPoint = ray.GetPointOnRay(result.mFraction);

    			JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(
					result.mSubShapeID2,
					worldPoint
				);

    			hitResult.point = Vector3<float>{
    				worldPoint.GetX(),
					worldPoint.GetY(),
					worldPoint.GetZ()
				};

    			hitResult.normal = Vector3<float>{
    				normal.GetX(),
					normal.GetY(),
					normal.GetZ()
				};

    			hitResult.distance = result.mFraction * maxDistance;
    			hitResult.bodyID = result.mBodyID;
    		}
    	}

    	return hitResult;
    }

	std::shared_ptr<PhysicsShape> PhysicsWorld::getOrCreateBox(const Vector3<float> &halfExtents)
	{
    	ShapeKey key;
    	key.type = EPhysicsShapeType::BOX;
    	key.scale = halfExtents;

    	auto it = m_shapeCache.find(key);
    	if (it != m_shapeCache.end())
    	{
    		if (auto existing = it->second.lock())
    			return existing;
    	}

    	auto shape = PhysicsShape::createBox(halfExtents);
    	m_shapeCache[key] = shape;

    	return shape;
	}

	std::shared_ptr<PhysicsShape> PhysicsWorld::getOrCreateSphere(float radius)
    {
    	ShapeKey key;
    	key.type = EPhysicsShapeType::SPHERE;
    	key.radius = radius;

    	auto it = m_shapeCache.find(key);
    	if (it != m_shapeCache.end())
    	{
    		if (auto existing = it->second.lock())
    			return existing;
    	}

    	auto shape = PhysicsShape::createSphere(radius);
    	if (!shape)
    		return nullptr;

    	m_shapeCache[key] = shape;
    	return shape;
    }

	std::shared_ptr<PhysicsShape> PhysicsWorld::getOrCreateCylinder(float halfHeight, float radius)
    {
    	ShapeKey key;
    	key.type = EPhysicsShapeType::CYLINDER;
    	key.height = halfHeight;
    	key.radius = radius;

    	auto it = m_shapeCache.find(key);
    	if (it != m_shapeCache.end())
    	{
    		if (auto existing = it->second.lock())
    			return existing;
    	}

    	auto shape = PhysicsShape::createCylinder(halfHeight, radius);
    	if (!shape)
    		return nullptr;

    	m_shapeCache[key] = shape;
    	return shape;
    }

	std::shared_ptr<PhysicsShape> PhysicsWorld::getOrCreateCapsule(float halfHeight, float radius)
    {
    	ShapeKey key;
    	key.type = EPhysicsShapeType::CAPSULE;
    	key.height = halfHeight;
    	key.radius = radius;

    	auto it = m_shapeCache.find(key);
    	if (it != m_shapeCache.end())
    	{
    		if (auto existing = it->second.lock())
    			return existing;
    	}

    	auto shape = PhysicsShape::createCapsule(halfHeight, radius);
    	if (!shape)
    		return nullptr;

    	m_shapeCache[key] = shape;
    	return shape;
    }

	std::shared_ptr<PhysicsShape> PhysicsWorld::getOrCreateScaled(std::shared_ptr<PhysicsShape> baseShape, Vector3<float> scale)
	{
    	if (!baseShape)
    		return nullptr;

    	ShapeKey key;
    	key.type = baseShape->getType();
    	key.scale = scale;

    	auto it = m_shapeCache.find(key);
    	if (it != m_shapeCache.end())
    	{
    		if (auto existing = it->second.lock())
    			return existing;

    		m_shapeCache.erase(it);
    	}

    	auto shape = PhysicsShape::createScaled(baseShape, scale);
    	if (!shape)
    		return nullptr;

    	m_shapeCache[key] = shape;
    	return shape;
	}
} // namespace gcep
