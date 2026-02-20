#pragma once

// Core
#include "Engine/Core/Maths/Vector3.hpp"
#include "Engine/Core/Maths/Quaternion.hpp"

namespace gcep
{
    enum class EShapeType
    {
        CUBE,
        SPHERE,
        CYLINDER
    };

    enum class EMotionType
    {
        STATIC,
        DYNAMIC,
        KINEMATIC
    };

    enum class ELayers
    {
        NON_MOVING = 0,
        MOVING = 1
    };

    class ObjectPhysicsData
    {
    public:
        ObjectPhysicsData(EShapeType type);
        ~ObjectPhysicsData();

#pragma region Setters

        void setPosition(const Vector3<float>& position);
        void setRotation(const Quaternion& rotation);
        void setScale(const Vector3<float>& scale);

        void setLinearVelocity(Vector3<float> velocity);
        void setAngularVelocity(Vector3<float> velocity);

        void setShapeType(EShapeType type);
        void setMotionType(EMotionType type);
        void setLayers(ELayers layers);

#pragma endregion

#pragma region Getters

        Vector3<float> getPosition() const;
        Quaternion getRotation() const;
        Vector3<float> getScale() const;

        Vector3<float> getLinearVelocity() const;
        Vector3<float> getAngularVelocity() const;

        EShapeType getShapeType() const;
        EMotionType getMotionType() const;
        ELayers getLayers() const;

#pragma endregion

    private:
        Vector3<float> m_position;
        Quaternion m_rotation;
        Vector3<float> m_scale;

        Vector3<float> m_angularVelocity;
        Vector3<float> m_linearVelocity;

        EShapeType m_shapeType;
        EMotionType m_motionType;
        ELayers m_layers;
    };
}

