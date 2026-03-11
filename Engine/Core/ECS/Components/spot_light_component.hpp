#pragma once
#include <Engine/Core/ECS/headers/component_registry.hpp>
#include <glm/glm.hpp>

namespace gcep::ECS
{
    struct SpotLightComponent
    {
        Vector3<float> position       = { 0.0f,  1.0f, 0.0f }; ///< World-space origin.
        Vector3<float> direction      = { 0.0f, -1.0f, 0.0f }; ///< Unit vector pointing down the cone axis.
        Vector3<float> color          = { 1.0f,  1.0f, 1.0f }; ///< Linear-space RGB color.
        float     intensity      = 1.0f;                   ///< Luminous intensity scalar.
        float     radius         = 20.0f;                  ///< Maximum influence radius in world units.
        float     innerCutoffDeg = 15.0f;                  ///< Full-brightness cone half-angle (degrees).
        float     outerCutoffDeg = 30.0f;                  ///< Zero-brightness cone half-angle (degrees).
        bool      showGizmo      = true;                   ///< When false the cone wireframe gizmo is hidden.
        static inline bool _gcep_registered =
                ComponentRegistry::instance().reg<SpotLightComponent>();
    };

}
