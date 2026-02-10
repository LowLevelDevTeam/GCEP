#include "PhysicalWorld.hpp"
#include "PhysicalWorld.hpp"

//STL
#include <cstdarg>
#include<iostream>
#include <thread>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include "Jolt/Physics/Collision/Shape/CylinderShape.h"

namespace gcep {

	// Callback for traces, connect this to your own trace function if you have one
	static void TraceImpl(const char *inFMT, ...)
	{
		// Format the message
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		va_end(list);

		// Print to the TTY
		std::cout << buffer << std::endl;
	}

#ifdef JPH_ENABLE_ASSERTS

	// Callback for asserts, connect this to your own assert handler if you have one
	static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, glm::uint inLine)
	{
		// Print to the TTY
		std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr? inMessage : "") << std::endl;

		// Breakpoint
		return true;
	};

#endif // JPH_ENABLE_ASSERTS

	namespace Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
	};


	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
	};

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
    	// Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
    	// This needs to be done before any other Jolt function is called.
    	JPH::RegisterDefaultAllocator();

    	// Install trace and assert callbacks
    	JPH::Trace = TraceImpl;
    	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
		// It is not directly used in this example but still required.
		JPH::Factory::sInstance = new JPH::Factory();

		// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
		// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
		// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
		JPH::RegisterTypes();

		// We need a temp allocator for temporary allocations during the physics update. We're
		// pre-allocating 10 MB to avoid having to do allocations during the physics update.
		// B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
		// If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
		// malloc / free.
		JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

		// We need a job system that will execute physics jobs on multiple threads. Typically
		// you would implement the JobSystem interface yourself and let Jolt Physics run on top
		// of your own job scheduler. JobSystemThreadPool is an example implementation.
		JPH::obSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

		// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const glm::uint cMaxBodies = 1024;

		// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
		const glm::uint cNumBodyMutexes = 0;

		// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
		// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
		// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const glm::uint cMaxBodyPairs = 1024;

		// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
		// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
		const glm::uint cMaxContactConstraints = 1024;

		// Create mapping table from object layer to broadphase layer
		// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
		// Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
		JPH::BPLayerInterfaceImpl broad_phase_layer_interface;

		// Create class that filters object vs broadphase layers
		// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
		// Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
		JPH::ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;

		// Create class that filters object vs object layers
		// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
		// Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
		JPH::ObjectLayerPairFilterImpl object_vs_object_layer_filter;

		m_physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);
    }

    void PhysicsWorld::step(float deltaTime)
    {

    }

    void PhysicsWorld::shutdownWorld()
    {

    }

    void PhysicsWorld::createBody(const PhysicsBodyDesc& desc) const
    {
    	JPH::ShapeRefC shape;

    	switch (desc.shape) {
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

    	JPH::BodyCreationSettings sphereSettings(shape, desc.position, desc.rotation, JPH::EMotionType::Static, Layers::NON_MOVING);

    	JPH::Body *sphere = m_physicsSystem->GetBodyInterface().CreateBody(sphereSettings);
    	m_physicsSystem->GetBodyInterface().AddBody(sphere->GetID(), JPH::EActivation::Activate);
    }

    void PhysicsWorld::destroyBody(const JPH::BodyID &body_id) const
    {
    	m_physicsSystem->GetBodyInterface().RemoveBody(body_id);
    	m_physicsSystem->GetBodyInterface().DestroyBody(body_id);
    }

}
