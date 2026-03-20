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
#include <ranges>

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

        auto buildInfo = [&](ECS::EntityID entity, bool isTrigger) -> BodyCreateInfo
        {
            auto& transform = registry->getComponent<ECS::Transform>(entity);
            BodyCreateInfo info;
            info.position = transform.position;
            info.rotation = transform.rotation;
            info.isTrigger = isTrigger;

            if (registry->hasComponent<ECS::RigidBodyComponent>(entity))
            {
                auto& rb = registry->getComponent<ECS::RigidBodyComponent>(entity);
                switch (rb.motionType)
                {
                    case ECS::EMotionType::DYNAMIC:   info.motionType = JPH::EMotionType::Dynamic;   break;
                    case ECS::EMotionType::KINEMATIC: info.motionType = JPH::EMotionType::Kinematic; break;
                    default:                          info.motionType = JPH::EMotionType::Static;    break;
                }
                info.mass = rb.mass;
                info.linearDamping = rb.linearDamping;
                info.angularDamping = rb.angularDamping;

                info.gravityFactor = rb.gravityFactor;
            }
            else
            {
                info.motionType = JPH::EMotionType::Static;
            }
            return info;
        };

        m_bodyToEntity.clear();
        m_triggerBodies.clear();

        auto registerBody = [&](ECS::EntityID entity, JPH::BodyID id, bool isTrigger)
        {
            const uint32_t key = id.GetIndexAndSequenceNumber();
            m_bodyToEntity[key] = entity;
            if (isTrigger) m_triggerBodies.insert(key);
        };

        for (auto entity : registry->view<ECS::BoxColliderComponent>())
        {
            auto& col = registry->getComponent<ECS::BoxColliderComponent>(entity);
            auto shape = m_world->getOrCreateBox(col.halfExtents);
            if (!shape) continue;

            auto& runtime = registry->addComponent<ECS::PhysicsRuntimeData>(entity);
            JPH::BodyID id = m_world->createBody(*shape, buildInfo(entity, col.isTrigger));
            runtime.bodyID = id.GetIndexAndSequenceNumber();
            registerBody(entity, id, col.isTrigger);
        }
        for (auto entity : registry->view<ECS::SphereColliderComponent>())
        {
            auto& col = registry->getComponent<ECS::SphereColliderComponent>(entity);
            auto shape = m_world->getOrCreateSphere(col.radius);
            if (!shape) continue;

            auto& runtime = registry->addComponent<ECS::PhysicsRuntimeData>(entity);
            JPH::BodyID id = m_world->createBody(*shape, buildInfo(entity, col.isTrigger));
            runtime.bodyID = id.GetIndexAndSequenceNumber();
            registerBody(entity, id, col.isTrigger);
        }

        for (auto entity : registry->view<ECS::CapsuleColliderComponent>())
        {
            auto& col = registry->getComponent<ECS::CapsuleColliderComponent>(entity);
            auto shape = m_world->getOrCreateCapsule(col.halfHeight, col.radius);
            if (!shape) continue;

            auto& runtime = registry->addComponent<ECS::PhysicsRuntimeData>(entity);
            JPH::BodyID id = m_world->createBody(*shape, buildInfo(entity, col.isTrigger));
            runtime.bodyID = id.GetIndexAndSequenceNumber();
            registerBody(entity, id, col.isTrigger);
        }

        for (auto entity : registry->view<ECS::CylinderColliderComponent>())
        {
            auto& col = registry->getComponent<ECS::CylinderColliderComponent>(entity);
            auto shape = m_world->getOrCreateCylinder(col.halfHeight, col.radius);
            if (!shape) continue;

            auto& runtime = registry->addComponent<ECS::PhysicsRuntimeData>(entity);
            JPH::BodyID id = m_world->createBody(*shape, buildInfo(entity, col.isTrigger));
            runtime.bodyID = id.GetIndexAndSequenceNumber();
            registerBody(entity, id, col.isTrigger);
        }

        m_world->m_physicsSystem->OptimizeBroadPhase();
    }

    void PhysicsSystem::update(float dt)
    {
        syncTransformsToPhysics(*registry);
        applyForces(*registry);
        syncComponentsVelocitiesToPhysics(*registry);

        m_world->step(dt);

        // Map raw contact events (BodyID) to ECS EntityIDs
        auto rawEnter = m_world->m_contactListener->flushEnterEvents();
        auto rawExit  = m_world->m_contactListener->flushExitEvents();

        m_enterEvents.clear();
        m_exitEvents.clear();

        for (auto& e : rawEnter)
        {
            auto it1 = m_bodyToEntity.find(e.body1.GetIndexAndSequenceNumber());
            auto it2 = m_bodyToEntity.find(e.body2.GetIndexAndSequenceNumber());
            if (it1 == m_bodyToEntity.end() || it2 == m_bodyToEntity.end()) continue;

            const bool trigger = m_triggerBodies.count(e.body1.GetIndexAndSequenceNumber()) ||
                                 m_triggerBodies.count(e.body2.GetIndexAndSequenceNumber());

            // Push one event per participant so each script sees itself as "self"
            m_enterEvents.push_back({it1->second, it2->second, e.contactX, e.contactY, e.contactZ,  e.normalX,  e.normalY,  e.normalZ,  trigger});
            m_enterEvents.push_back({it2->second, it1->second, e.contactX, e.contactY, e.contactZ, -e.normalX, -e.normalY, -e.normalZ, trigger});
        }

        for (auto& e : rawExit)
        {
            auto it1 = m_bodyToEntity.find(e.body1.GetIndexAndSequenceNumber());
            auto it2 = m_bodyToEntity.find(e.body2.GetIndexAndSequenceNumber());
            if (it1 == m_bodyToEntity.end() || it2 == m_bodyToEntity.end()) continue;

            const bool trigger = m_triggerBodies.count(e.body1.GetIndexAndSequenceNumber()) ||
                                 m_triggerBodies.count(e.body2.GetIndexAndSequenceNumber());

            m_exitEvents.push_back({it1->second, it2->second, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, trigger});
            m_exitEvents.push_back({it2->second, it1->second, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, trigger});
        }

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

        m_bodyToEntity.clear();
        m_triggerBodies.clear();
        m_enterEvents.clear();
        m_exitEvents.clear();
    }

    void PhysicsSystem::setRegistry(ECS::Registry* reg)
    {
        registry = reg;
    }

    void PhysicsSystem::syncPhysicsToTransforms(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::Transform, ECS::RigidBodyComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& transform = view.get<ECS::Transform>(entity);
            auto& rigid   = view.get<ECS::RigidBodyComponent>(entity);
            auto& runtime   = view.get<ECS::PhysicsRuntimeData>(entity);

            if (rigid.motionType != ECS::EMotionType::DYNAMIC) continue;

            JPH::BodyID bodyID(runtime.bodyID);
            if (bodyID.IsInvalid()) continue;

            JPH::RVec3 pos = bodyInterface.GetPosition(bodyID);
            JPH::Quat  rot = bodyInterface.GetRotation(bodyID);

            transform.position = Vector3Convertor::FromJolt(pos);
            transform.rotation = QuaternionConvertor::FromJolt(rot);
            transform.eulerRadians = transform.rotation.ToEuler();
        }
    }

    void PhysicsSystem::syncTransformsToPhysics(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::Transform, ECS::RigidBodyComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& transform = view.get<ECS::Transform>(entity);
            auto& physics   = view.get<ECS::RigidBodyComponent>(entity);
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

    void PhysicsSystem::syncPhysicsVelocitiesToComponents(ECS::Registry& reg)
    {
        auto view = reg.view<ECS::RigidBodyComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<ECS::RigidBodyComponent>(entity);
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
        auto view = reg.view<ECS::RigidBodyComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<ECS::RigidBodyComponent>(entity);
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
        auto view = reg.view<ECS::RigidBodyComponent, ECS::PhysicsRuntimeData>();
        auto& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        for (auto entity : view)
        {
            auto& physics = view.get<ECS::RigidBodyComponent>(entity);
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
