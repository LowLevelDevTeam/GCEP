#include "physics_system.hpp"

#include <memory>
#include <iostream>

#include "physics_world.hpp"
#include "Component/transform_component.hpp"
#include "Maths/Utils/vector3_convertor.hpp"
#include "Maths/Utils/quaternion_convertor.hpp"

namespace gcep
{
    PhysicsSystem::PhysicsSystem()
    {
        std::cout << "Physics System created\n";
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
        auto view = registry->view<TransformComponent, PhysicsComponent>();

        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& physics = view.get<PhysicsComponent>(entity);

            m_world->createBody(transform, physics, physics.m_bodyIDRef);
        }
        m_world->m_physicsSystem->OptimizeBroadPhase();
    }

    void PhysicsSystem::update(float dt)
    {
        syncTransformsToPhysics(*registry);
        applyForces(*registry);
        syncComponentsVelocitiesToPhysics(*registry);

        m_world->step(dt);

        syncPhysicsToTransforms(*registry);
        syncPhysicsVelocitiesToComponents(*registry);
    }

    void PhysicsSystem::stopSimulation()
    {
        auto view = registry->view<PhysicsComponent>();

        for (ECS::EntityID id : view) {
            auto& pc = view.get<PhysicsComponent>(id); // getter
            m_world->destroyBody(pc.m_bodyIDRef);
        }
    }

    void PhysicsSystem::setRegistry(ECS::Registry* reg)
    {
        registry = reg;
    }

    void PhysicsSystem::syncPhysicsToTransforms(ECS::Registry& reg)
    {
        auto view = reg.view<TransformComponent, PhysicsComponent>();

        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& transform = view.get<TransformComponent>(entity);
            auto& physics = view.get<PhysicsComponent>(entity);

            if (physics.motionType != EMotionType::DYNAMIC)
                continue;

            if (physics.m_bodyIDRef.IsInvalid())
                continue;

            JPH::RVec3 pos = bodyInterface.GetPosition(physics.m_bodyIDRef);
            JPH::Quat rot = bodyInterface.GetRotation(physics.m_bodyIDRef);

            transform.position = Vector3Convertor::FromJolt(pos);
            transform.rotation = QuaternionConvertor::FromJolt(rot);
        }
    }

    void PhysicsSystem::syncTransformsToPhysics(ECS::Registry& reg)
    {
        auto view = reg.view<TransformComponent, PhysicsComponent>();

        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& transform = view.get<TransformComponent>(entity);
            auto& physics = view.get<PhysicsComponent>(entity);

            if (physics.motionType != EMotionType::KINEMATIC)
                continue;

            if (physics.m_bodyIDRef.IsInvalid())
                continue;

            Quaternion normalizedRot = transform.rotation.Normalized();

            bodyInterface.SetPositionAndRotation(
                physics.m_bodyIDRef,
                Vector3Convertor::ToJolt(transform.position),
                  QuaternionConvertor::ToJolt(transform.rotation),
                JPH::EActivation::Activate
            );
        }
    }

    void PhysicsSystem::syncPhysicsVelocitiesToComponents(ECS::Registry& reg)
    {
        auto view = reg.view<PhysicsComponent>();

        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<PhysicsComponent>(entity);

            if (physics.motionType != EMotionType::DYNAMIC)
                continue;

            if (physics.m_bodyIDRef.IsInvalid())
                continue;

            JPH::Vec3 linear = bodyInterface.GetLinearVelocity(physics.m_bodyIDRef);
            JPH::Vec3 angular = bodyInterface.GetAngularVelocity(physics.m_bodyIDRef);

            physics.linearVelocity = Vector3Convertor::FromJolt(linear);
            physics.angularVelocity = Vector3Convertor::FromJolt(angular);
        }
    }

    void PhysicsSystem::syncComponentsVelocitiesToPhysics(ECS::Registry& reg)
    {
        auto view = reg.view<PhysicsComponent>();

        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<PhysicsComponent>(entity);

            if (physics.motionType != EMotionType::DYNAMIC)
                continue;

            if (physics.m_bodyIDRef.IsInvalid())
                continue;

            bodyInterface.SetLinearVelocity(
                physics.m_bodyIDRef,
                Vector3Convertor::ToJolt(physics.linearVelocity)
            );

            bodyInterface.SetAngularVelocity(
                physics.m_bodyIDRef,
                Vector3Convertor::ToJolt(physics.angularVelocity)
            );
        }
    }

    void PhysicsSystem::applyForces(ECS::Registry& reg)
    {
        auto view = reg.view<PhysicsComponent>();

        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<PhysicsComponent>(entity);

            if (physics.motionType != EMotionType::DYNAMIC)
                continue;

            if (physics.m_bodyIDRef.IsInvalid())
                continue;

            bodyInterface.ActivateBody(physics.m_bodyIDRef);

            // Force continue
            bodyInterface.AddForce(physics.m_bodyIDRef,Vector3Convertor::ToJolt(physics.force));

            // Impulse instantané
            bodyInterface.AddImpulse(physics.m_bodyIDRef,Vector3Convertor::ToJolt(physics.impulse));

            // Torque
            bodyInterface.AddTorque(physics.m_bodyIDRef,Vector3Convertor::ToJolt(physics.torque));

            // Angular impulse
            bodyInterface.AddAngularImpulse(physics.m_bodyIDRef,Vector3Convertor::ToJolt(physics.angularImpulse));

            // Reset impulses (one frame)
            physics.impulse = {0,0,0};
            physics.angularImpulse = {0,0,0};
        }
    }

    RaycastHit PhysicsSystem::raycast(const Vector3<float>& origin, const Vector3<float>& direction, float maxDistance)
    {
        return m_world->raycast(origin, direction, maxDistance);
    }
/*
    void PhysicsSystem::addImpulse(const JPH::BodyID* bodyID, const Vector3<float> impulse) const
    {
        std::cout << "[Physics System] Calling addImpulse\n";

        std::cout << "[Physics System] addImpulse: BodyID index="
          << bodyID->GetIndex()
          << " seq=" << (int)bodyID->GetSequenceNumber()
          << "\n";

        if (bodyID->IsInvalid())
        {
            std::cout << "[Physics System] addImpulse: invalid BodyID\n";
            return;
        }

        //const JPH::BodyLockWrite lock(m_world->m_physicsSystem->GetBodyLockInterface(), bodyID);
        //if (!lock.Succeeded()) { std::cout << "Failed\n"; return;};

        std::cout << "[Physics System] Adding Impulse\n";

        JPH::BodyInterface& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        if (!bodyInterface.IsAdded(*bodyID))
        {
            std::cout << "[Physics System] Body not added to physics world\n";
            return;
        }

        bodyInterface.ActivateBody(*bodyID);
        bodyInterface.AddImpulse(*bodyID, Vector3Convertor::ToJolt(impulse));
    }

    void PhysicsSystem::addForce(const JPH::BodyID* bodyID, const Vector3<float> force) const
    {
        std::cout << "[Physics System] Calling addForce\n";

        if (bodyID->IsInvalid())
        {
            std::cout << "[Physics System] addImpulse: invalid BodyID\n";
            return;
        }

        //const JPH::BodyLockWrite lock(m_world->m_physicsSystem->GetBodyLockInterface(), bodyID);
        //if (!lock.Succeeded()) { std::cout << "Failed\n"; return;}

        std::cout << "[Physics System] Adding Force\n";

        JPH::BodyInterface& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        if (!bodyInterface.IsAdded(*bodyID))
        {
            std::cout << "[Physics System] Body not added to physics world\n";
            return;
        }

        bodyInterface.ActivateBody(*bodyID);
        bodyInterface.AddForce(*bodyID, Vector3Convertor::ToJolt(force));
    }
    */
} // gcep