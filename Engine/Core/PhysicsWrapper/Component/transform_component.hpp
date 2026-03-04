#pragma once

#include "Engine/Core/Maths/vector3.hpp"
#include "Engine/Core/Maths/quaternion.hpp"

namespace gcep {

    /**
     * @struct TransformComponent
     * @brief Represents the position, rotation, and scale of an entity in the world.
     *
     * Used by the ECS to store spatial information for an entity.
     * Typically synchronized with the physics system or rendering system.
     */
    struct TransformComponent
    {
        alignas(16) Vector3<float> position{0.f, 0.f, 0.f};

        // Single authoritative rotation
        alignas(16) Quaternion rotation{1.f, 0.f, 0.f, 0.f};

        alignas(16) Vector3<float> scale{1.f, 1.f, 1.f};

        // Optional: editor-only Euler cache
        alignas(16) Vector3<float> eulerRadians{0.f, 0.f, 0.f};
    };

} // namespace gcep