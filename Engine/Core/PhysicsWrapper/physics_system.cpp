#include "physics_system.hpp"

#include <memory>
#include <iostream>

#include "physics_world.hpp"
#include "Component/transform_component.hpp"
#include "Maths/Utils/vector3_convertor.hpp"

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
        m_world->step(dt);
        // Sync dynamic bodies → Transform
        syncPhysicsToTransforms(*registry);

        // Sync kinematic bodies ← Transform
        syncTransformsToPhysics(*registry);
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

            transform.position = {
                (float)pos.GetX(),
                (float)pos.GetY(),
                (float)pos.GetZ()
            };

            transform.rotation = {
                rot.GetW(),
                rot.GetX(),
                rot.GetY(),
                rot.GetZ()
            };
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

            bodyInterface.SetPositionAndRotation(
                physics.m_bodyIDRef,
                JPH::RVec3(transform.position.x,
                           transform.position.y,
                           transform.position.z),
                JPH::Quat(transform.rotation.x,
                          transform.rotation.y,
                          transform.rotation.z,
                          transform.rotation.w),
                JPH::EActivation::Activate
            );
        }
    }

    RaycastHit PhysicsSystem::raycast(const Vector3<float>& origin, const Vector3<float>& direction, float maxDistance)
    {
        return m_world->raycast(origin, direction, maxDistance);
    }

    void PhysicsSystem::addImpulse(const JPH::BodyID bodyID, const Vector3<float> impulse) const
    {
        JPH::BodyInterface& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        bodyInterface.ActivateBody(bodyID);
        bodyInterface.AddImpulse(bodyID, Vector3Convertor::ToJolt(impulse));
    }

    void PhysicsSystem::addForce(const JPH::BodyID bodyID, const Vector3<float> force) const
    {
        std::cout << "[Physics System] Adding Force\n";

        JPH::BodyInterface& bodyInterface = m_world->m_physicsSystem->GetBodyInterface();

        bodyInterface.ActivateBody(bodyID);
        bodyInterface.AddForce(bodyID, Vector3Convertor::ToJolt(force));

    }
} // gcep