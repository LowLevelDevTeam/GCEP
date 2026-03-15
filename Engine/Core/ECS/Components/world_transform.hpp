#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>

namespace gcep::ECS
{
    struct WorldTransform
    {
        alignas(16) Vector3<float> position    { 0.f, 0.f, 0.f };
        alignas(16) Vector3<float> scale       { 1.f, 1.f, 1.f };
        alignas(16) Quaternion     rotation    { 1.f, 0.f, 0.f, 0.f };
        alignas(16) Vector3<float> eulerRadians{ 0.f, 0.f, 0.f };
        
        static inline bool _gcep_registered =
        gcep::ECS::ComponentRegistry::instance().reg<WorldTransform>();
    };
} // namespace gcep::ECS
