#include "physics_component.hpp"

#include "Engine/Core/Maths/Utils/vector3_convertor.hpp"
#include "Engine/Core/Maths/Utils/quaternion_convertor.hpp"

#include "Engine/Core/Entity-Component-System/headers/registry.hpp"

namespace gcep
{
    // when simulation is on
    void PhysicsComponentManager::updateAllComponents()
    {
        // Temp registry (Registry::getInstance()) dans le futur
        Registry reg;

        auto view = reg.partialView<PhysicsComponent>();

        for (EntityID id : view)
        {
            auto& transform = view.get<PhysicsComponent>(id); // getter
        }
    }
} // gcep

/**
*       void PhysicsComponent::teleport(const Vector3<float> newPos)
        {
            m_physicsSystem->GetBodyInterface().SetPosition(m_bodyIDRef, Vector3Converter::ToJolt(m_position), JPH::EActivation::Activate);
        }


        JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

        m_position = Vector3Converter::FromJolt(bodyInterface.GetPosition(m_bodyIDRef));
        m_rotation = QuaternionConverter::FromJolt(bodyInterface.GetRotation(m_bodyIDRef));

        bodyInterface.SetAngularVelocity(m_bodyIDRef, Vector3Converter::ToJolt(m_angularVelocity));
        bodyInterface.SetLinearVelocity(m_bodyIDRef, Vector3Converter::ToJolt(m_linearVelocity));
 */