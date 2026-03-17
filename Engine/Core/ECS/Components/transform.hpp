#pragma once

// Internals
#include <Maths/vector3.hpp>
#include <Maths/quaternion.hpp>
#include <ECS/headers/component_registry.hpp>

namespace gcep::ECS
{
    struct Transform
    {
        alignas(16) Vector3<float> position    { 0.f, 0.f, 0.f };
        alignas(16) Vector3<float> scale       { 1.f, 1.f, 1.f };
        alignas(16) Quaternion     rotation    { 1.f, 0.f, 0.f, 0.f };
        alignas(16) Vector3<float> eulerRadians{ 0.f, 0.f, 0.f };

        static inline bool _gcep_registered =
            ComponentRegistry::instance().reg<Transform>();
    };
} // namespace gcep::ECS
