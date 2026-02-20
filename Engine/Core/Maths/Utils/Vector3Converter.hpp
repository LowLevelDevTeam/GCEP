#pragma once

// Jolt
#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

// Engine
#include "Engine/Core/Maths/Vector3.hpp"

// STL
#include <type_traits>

namespace gcep
{
    /*
    * Utility class used to convert gcep::Vector3 to and from JPH::Vec3.
    * This class is stateless and must not be instantiated.
    * Jolt dependency is strictly confined to this file.
    */
    class Vector3Converter
    {
    public:

        // Jolt Conversion

        template<typename T>
        requires std::is_floating_point_v<T>
        static JPH::Vec3 ToJolt(const Vector3<T>& vector)
        {
            return JPH::Vec3(
                static_cast<float>(vector.x),
                static_cast<float>(vector.y),
                static_cast<float>(vector.z)
            );
        }

        template<typename T = float>
        static Vector3<T> FromJolt(const JPH::Vec3& vector)
        {
            return Vector3<T>(
                static_cast<T>(vector.GetX()),
                static_cast<T>(vector.GetY()),
                static_cast<T>(vector.GetZ())
            );
        }

        // TODO: add glm conversion

    private:

        // Creating an instance of this class is prohibited
        Vector3Converter() = delete;
    };
} // gcep