#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>

namespace gcep::ECS
{
    struct CameraComponent
    {
        float fovYDeg = 60.f;
        float nearZ   = 0.01f;
        float farZ    = 1000.f;
        bool isMainCamera = true;  ///< If true, this entity controls the editor camera

        static inline bool _gcep_registered =
                ComponentRegistry::instance().reg<CameraComponent>();
    };
} // namespace gcep::ECS