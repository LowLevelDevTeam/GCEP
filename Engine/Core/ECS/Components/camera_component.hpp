#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>
#include <Maths/vector3.hpp>

namespace gcep::ECS
{
    struct CameraComponent
    {
        float fovYDeg     = 60.f;
        float nearZ       = 0.01f;
        float farZ        = 1000.f;

        /// If true, this entity's camera drives the active game/scene view.
        bool  isMainCamera = true;

        Vector3<float> position = { 0.f, 0.f, 0.f };

        float pitch = 0.f;
        float yaw = -180.f;

        static inline bool _gcep_registered =
                ComponentRegistry::instance().reg<CameraComponent>();
    };
} // namespace gcep::ECS