#pragma once

#include "Engine/Core/Maths/vector3.hpp"
#include "Engine/Core/Maths/quaternion.hpp"

namespace gcep
{
    struct TransformComponent
    {
        Vector3<float> position{0.f, 0.f, 0.f};
        Quaternion rotation{1.f, 0.f, 0.f, 0.f};
        Vector3<float> scale{0.f, 0.f, 0.f};

        // TOADD: Reference to owner
    };

    class TransformComponentManager
    {
    public:
        void updateAllComponents(); //=> get tous les transform components => recuperer valeurs => faire un truc => update les valeurs
    };
} // gcep
