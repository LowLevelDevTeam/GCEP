#pragma once
#include <memory>

#include "Jolt/Jolt.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Engine/Core/Maths/Vector3.hpp"
#include "Engine/Core/Maths/Quaternion.hpp"
#include "Engine/Core/PhysicsWrapper/PhysicsShape.hpp"
#include "Engine/Core/PhysicsWrapper/Component/TransformComponent.hpp"

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

    class PhysicsComponent
    {

    public:

        void teleport(Vector3<float> newPos);

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

        [[nodiscard]] Vector3<float> getPosition() const;
        [[nodiscard]] Quaternion getRotation() const;
        [[nodiscard]] Vector3<float> getScale() const;

        [[nodiscard]] Vector3<float> getLinearVelocity() const;
        [[nodiscard]] Vector3<float> getAngularVelocity() const;

        [[nodiscard]] EShapeType getShapeType() const;
        [[nodiscard]] EMotionType getMotionType() const;
        [[nodiscard]] ELayers getLayers() const;

#pragma endregion

    private :
        JPH::BodyID	m_bodyIDRef;
        std::shared_ptr<JPH::PhysicsSystem> m_physicsSystem = nullptr;

        Vector3<float> m_position = Vector3<float>(0.f,0.f,0.f);
        Quaternion m_rotation = Quaternion(1.f,0.f,0.f,0.f);
        Vector3<float> m_scale = Vector3<float>(1.f,1.f,1.f);
        Vector3<float> m_linearVelocity = Vector3<float>(0.f,0.f,0.f);
        Vector3<float> m_angularVelocity = Vector3<float>(0.f,0.f,0.f);

        // To put in IMGUI because we don't want the player to touch to these 3 while simulating

        EShapeType m_shapeType = EShapeType::CUBE;
        EMotionType m_motionType = EMotionType::STATIC;
        ELayers m_layers = ELayers::NON_MOVING;

        void updateData();

    };
} // gcep
