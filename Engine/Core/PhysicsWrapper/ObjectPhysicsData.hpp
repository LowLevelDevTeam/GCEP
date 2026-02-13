#pragma once

#include <array>

using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;

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

        void setPosition(const Vec3& position);
        void setRotation(const Quat& rotation);
        void setScale(const Vec3& scale);

        void setShapeType(EShapeType type);
        void setMotionType(EMotionType type);
        void setLayers(ELayers layers);

#pragma endregion

#pragma region Getters

        Vec3 getPosition() const;
        Quat getRotation() const;
        Vec3 getScale() const;

        EShapeType getShapeType() const;
        EMotionType getMotionType() const;
        ELayers getLayers() const;

#pragma endregion

    private:
        Vec3 m_position;
        Quat m_rotation;
        Vec3 m_scale;

        EShapeType m_shapeType;
        EMotionType m_motionType;
        ELayers m_layers;
    };
}

