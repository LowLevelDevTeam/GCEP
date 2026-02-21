
#include "PhysicsComponent.hpp"

#include "Engine/Core/Maths/Utils/Vector3Converter.hpp"
#include "Engine/Core/Maths/Utils/QuaternionConverter.hpp"

namespace gcep {
    void PhysicsComponent::teleport(const Vector3<float> newPos) {
        m_physicsSystem->GetBodyInterface().SetPosition(m_bodyIDRef, Vector3Converter::ToJolt(m_position), JPH::EActivation::Activate);
    }

    // when simulation is on
    void PhysicsComponent::updateData() {
        JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

        m_position = Vector3Converter::FromJolt(bodyInterface.GetPosition(m_bodyIDRef));
        m_rotation = QuaternionConverter::FromJolt(bodyInterface.GetRotation(m_bodyIDRef));

        bodyInterface.SetAngularVelocity(m_bodyIDRef, Vector3Converter::ToJolt(m_angularVelocity));
        bodyInterface.SetLinearVelocity(m_bodyIDRef, Vector3Converter::ToJolt(m_linearVelocity));
    }

#pragma region Setters

    void PhysicsComponent::setPosition(const Vector3<float>& position)
    {
        m_position = position;
    }

    void PhysicsComponent::setRotation(const Quaternion& rotation)
    {
        m_rotation = rotation;
    }

    void PhysicsComponent::setScale(const Vector3<float>& scale)
    {
        m_scale = scale;
    }

    void PhysicsComponent::setLinearVelocity(Vector3<float> velocity)
    {
        m_linearVelocity = velocity;
    }

    void PhysicsComponent::setAngularVelocity(Vector3<float> velocity)
    {
        m_angularVelocity = velocity;
    }

    void PhysicsComponent::setMotionType(EMotionType motionType)
    {
        m_motionType = motionType;
    }

    void PhysicsComponent::setLayers(ELayers layers)
    {
        m_layers = layers;
    }

    void PhysicsComponent::setShapeType(EShapeType type)
    {
        m_shapeType = type;
    }

#pragma endregion

#pragma region Getters

    Vector3<float> PhysicsComponent::getPosition() const
    {
        return m_position;
    }

    Quaternion PhysicsComponent::getRotation() const
    {
        return m_rotation;
    }

    Vector3<float> PhysicsComponent::getScale() const
    {
        return m_scale;
    }

    Vector3<float> PhysicsComponent::getLinearVelocity() const
    {
        return m_linearVelocity;
    }

    Vector3<float> PhysicsComponent::getAngularVelocity() const
    {
        return m_angularVelocity;
    }

    EShapeType PhysicsComponent::getShapeType() const
    {
        return m_shapeType;
    }

    EMotionType PhysicsComponent::getMotionType() const
    {
        return m_motionType;
    }

    ELayers PhysicsComponent::getLayers() const
    {
        return m_layers;
    }

#pragma endregion

} // gcep