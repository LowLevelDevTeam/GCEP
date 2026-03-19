#include "physics_system.hpp"

// Internals
#include <ECS/Components/physics_component.hpp>
#include <Engine/Core/ECS/Components/transform.hpp>
#include <Log/log.hpp>
#include <Maths/Utils/quaternion_convertor.hpp>
#include <Maths/Utils/vector3_convertor.hpp>
#include <PhysicsWrapper/physics_world.hpp>

// STL
#include <memory>
#include <iostream>

namespace gcep
{
    PhysicsSystem::PhysicsSystem()
    {
        Log::info("Physics System created");
    }

    PhysicsSystem::~PhysicsSystem()
    {
        shutdown();
        Log::info("Physics system destroyed");
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
        auto view = registry->view<ECS::Transform, ECS::PhysicsComponent>();

        for (auto entity : view)
        {
            auto& transform = view.get<ECS::Transform>(entity);
            auto& physics   = view.get<ECS::RigidBodyComponent>(entity);
            auto& runtime   = registry->addComponent<ECS::PhysicsRuntimeData>(entity);

            JPH::BodyID bodyID;
            m_world->createBody(transform, {}, {}, bodyID);
            runtime.bodyID = bodyID.GetIndexAndSequenceNumber();
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
        auto view = registry->view<ECS::PhysicsRuntimeData>();

        for (ECS::EntityID id : view)
        {
            auto& runtime = view.get<ECS::PhysicsRuntimeData>(id);
            JPH::BodyID bodyID(runtime.bodyID);
            if (!bodyID.IsInvalid())
                m_world->destroyBody(bodyID);
            registry->removeComponent<ECS::PhysicsRuntimeData>(id);
        }
    }

    void PhysicsSystem::setRegistry(ECS::Registry* reg)
    {
        registry = reg;
    }

    void PhysicsSystem::syncPhysicsToTransforms(ECS::Registry& reg)
    {
        auto spawnBody = [&](auto& collider, ECS::EntityID entity)
        {
            BodyCreateInfo info
        }

        auto view = reg.view<ECS::Transform, ECS::PhysicsComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& transform = view.get<ECS::Transform>(entity);
            auto& physics   = view.get<ECS::PhysicsComponent>(entity);
            auto& runtime   = view.get<ECS::PhysicsRuntimeData>(entity);

            if (physics.motionType != ECS::EMotionType::DYNAMIC) continue;

            JPH::BodyID bodyID(runtime.bodyID);
            if (bodyID.IsInvalid()) continue;

            JPH::RVec3 pos = bodyInterface.GetPosition(bodyID);
            JPH::Quat  rot = bodyInterface.GetRotation(bodyID);

            transform.position = Vector3Convertor::FromJolt(pos);
            transform.rotation = QuaternionConvertor::FromJolt(rot);
        }
    }

    void PhysicsSystem::syncTransformsToPhysics(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::Transform, ECS::PhysicsComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& transform = view.get<ECS::Transform>(entity);
            auto& physics   = view.get<ECS::PhysicsComponent>(entity);
            auto& runtime   = view.get<ECS::PhysicsRuntimeData>(entity);

            if (physics.motionType != ECS::EMotionType::KINEMATIC) continue;

            JPH::BodyID bodyID(runtime.bodyID);
            if (bodyID.IsInvalid()) continue;

            bodyInterface.SetPositionAndRotation(
                bodyID,
                Vector3Convertor::ToJolt(transform.position),
                QuaternionConvertor::ToJolt(transform.rotation),
                JPH::EActivation::Activate
            );
        }
    }

    void PhysicsSystem::getColliderType(ECS::EntityID id)
    {
        if (entit)
    }

    void PhysicsSystem::syncPhysicsVelocitiesToComponents(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::PhysicsComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<ECS::PhysicsComponent>(entity);
            auto& runtime = view.get<ECS::PhysicsRuntimeData>(entity);

            if (physics.motionType != ECS::EMotionType::DYNAMIC) continue;

            JPH::BodyID bodyID(runtime.bodyID);
            if (bodyID.IsInvalid()) continue;

            JPH::Vec3 linear  = bodyInterface.GetLinearVelocity(bodyID);
            JPH::Vec3 angular = bodyInterface.GetAngularVelocity(bodyID);

            runtime.linearVelocity  = Vector3Convertor::FromJolt(linear);
            runtime.angularVelocity = Vector3Convertor::FromJolt(angular);
        }
    }

    void PhysicsSystem::syncComponentsVelocitiesToPhysics(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::PhysicsComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<ECS::PhysicsComponent>(entity);
            auto& runtime = view.get<ECS::PhysicsRuntimeData>(entity);

            if (physics.motionType != ECS::EMotionType::DYNAMIC) continue;

            JPH::BodyID bodyID(runtime.bodyID);
            if (bodyID.IsInvalid()) continue;

            bodyInterface.SetLinearVelocity(bodyID,
                Vector3Convertor::ToJolt(runtime.linearVelocity));
            bodyInterface.SetAngularVelocity(bodyID,
                Vector3Convertor::ToJolt(runtime.angularVelocity));
        }
    }

    void PhysicsSystem::applyForces(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::PhysicsComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<ECS::PhysicsComponent>(entity);
            auto& runtime = view.get<ECS::PhysicsRuntimeData>(entity);

            if (physics.motionType != ECS::EMotionType::DYNAMIC) continue;

            JPH::BodyID bodyID(runtime.bodyID);
            if (bodyID.IsInvalid()) continue;

            bodyInterface.ActivateBody(bodyID);
            bodyInterface.AddForce(bodyID,          Vector3Convertor::ToJolt(runtime.force));
            bodyInterface.AddImpulse(bodyID,        Vector3Convertor::ToJolt(runtime.impulse));
            bodyInterface.AddTorque(bodyID,         Vector3Convertor::ToJolt(runtime.torque));
            bodyInterface.AddAngularImpulse(bodyID, Vector3Convertor::ToJolt(runtime.angularImpulse));

            runtime.impulse        = { 0.f, 0.f, 0.f };
            runtime.angularImpulse = { 0.f, 0.f, 0.f };
        }
    }

    RaycastHit PhysicsSystem::raycast(const Vector3<float>& origin,
                                       const Vector3<float>& direction,
                                       float maxDistance)
    {
        return m_world->raycast(origin, direction, maxDistance);
    }
} // namespace gcep
