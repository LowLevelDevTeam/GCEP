#pragma once

#include "Engine/Core/Maths/Vector3.hpp"
#include "Engine/Core/Maths/Quaternion.hpp"

namespace gcep
{
    class TransformComponent
    {
    public:
        Vector3<float> position{0.f, 0.f, 0.f};
        Quaternion rotation{1.f, 0.f, 0.f, 0.f};
        Vector3<float> scale{0.f, 0.f, 0.f};

    private:
        void update();
    };
} // gcep
