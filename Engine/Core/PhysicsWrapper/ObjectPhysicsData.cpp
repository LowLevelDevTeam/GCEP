#include "ObjectPhysicsData.hpp"

namespace gcep
{
    ObjectPhysicsData::ObjectPhysicsData(const EShapeType type) : m_shapeType(type)
    {
        m_position = {0.0f, 0.0f, 0.0f};
        m_rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        m_scale = {1.0f, 1.0f, 1.0f};
        m_motionType = EMotionType::STATIC;
        m_layers = ELayers::NON_MOVING;
    }

    ObjectPhysicsData::~ObjectPhysicsData()
    {}

#pragma region Setters

    void ObjectPhysicsData::setPosition(const Vector3<float>& position)
    {
        m_position = position;
    }

    void ObjectPhysicsData::setRotation(const Quaternion& rotation)
    {
        m_rotation = rotation;
    }

    void ObjectPhysicsData::setScale(const Vector3<float>& scale)
    {
        m_scale = scale;
    }

    void ObjectPhysicsData::setLinearVelocity(Vector3<float> velocity)
    {
        m_linearVelocity = velocity;
    }

    void ObjectPhysicsData::setAngularVelocity(Vector3<float> velocity)
    {
        m_angularVelocity = velocity;
    }

    void ObjectPhysicsData::setMotionType(EMotionType motionType)
    {
        m_motionType = motionType;
    }

    void ObjectPhysicsData::setLayers(ELayers layers)
    {
        m_layers = layers;
    }

    void ObjectPhysicsData::setShapeType(EShapeType type)
    {
        m_shapeType = type;
    }

#pragma endregion

#pragma region Getters

    Vector3<float> ObjectPhysicsData::getPosition() const
    {
        return m_position;
    }

    Quaternion ObjectPhysicsData::getRotation() const
    {
        return m_rotation;
    }

    Vector3<float> ObjectPhysicsData::getScale() const
    {
        return m_scale;
    }

    Vector3<float> ObjectPhysicsData::getLinearVelocity() const
    {
        return m_linearVelocity;
    }

    Vector3<float> ObjectPhysicsData::getAngularVelocity() const
    {
        return m_angularVelocity;
    }

    EShapeType ObjectPhysicsData::getShapeType() const
    {
        return m_shapeType;
    }

    EMotionType ObjectPhysicsData::getMotionType() const
    {
        return m_motionType;
    }

    ELayers ObjectPhysicsData::getLayers() const
    {
        return m_layers;
    }

#pragma endregion

} // gcep
