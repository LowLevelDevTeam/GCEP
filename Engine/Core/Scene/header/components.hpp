#pragma once
#include "Maths/quaternion.hpp"
#include "Maths/vector3.hpp"

namespace gcep::SLS
{
    struct Transform
    {
        Vector3<float> position  = { 0.0f, 0.0f, 0.0f };
        Vector3<float> rotation =  {  0.0f, 0.0f, 0.0f };
        Vector3<float> scale =    { 1.0f, 1.0f, 1.0f };
    };


}

