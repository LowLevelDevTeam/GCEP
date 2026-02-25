#pragma once

#include "Engine/Core/Maths/vector3.hpp"
#include "Engine/Core/Maths/quaternion.hpp"

namespace gcep {

    struct TransformComponent
    {
        Vector3<float> position = Vector3<float>(0.f,0.f,0.f);
        Quaternion rotation = Quaternion(1.f,0.f,0.f,0.f);
        Vector3<float> scale = Vector3<float>(1.f,1.f,1.f);
    };
} // gcep
