#pragma once
#include <Engine/Core/ECS/headers/component_registry.hpp>
#include <glm/glm.hpp>

namespace gcep::ECS
{
    struct PointLightComponent
    {
        Vector3<float> position  = { 0.0f, 0.0f, 0.0f }; ///< World-space origin.
        Vector3<float> color     = { 1.0f, 1.0f, 1.0f }; ///< Linear-space RGB color.
        float     intensity = 1.0f;                 ///< Luminous intensity scalar (lm/sr equivalent).
        float     radius    = 10.0f;                ///< Maximum influence radius in world units.
        static inline bool _gcep_registered =
                ComponentRegistry::instance().reg<PointLightComponent>();
    };

}
