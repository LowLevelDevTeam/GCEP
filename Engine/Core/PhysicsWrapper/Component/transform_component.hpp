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
        /// World-space position of the entity.
        alignas(16) Vector3<float> position = Vector3<float>(0.f, 0.f, 0.f);

        /// World-space rotation of the entity.
        alignas(16) Quaternion rotation = Quaternion(1.f, 0.f, 0.f, 0.f);

        /// Local scale of the entity.
        alignas(16) Vector3<float> scale = Vector3<float>(1.f, 1.f, 1.f);
    };

} // namespace gcep