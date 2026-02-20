#pragma once

// Jolt
#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>

// Engine
#include "Engine/Core/Maths/Quaternion.hpp"

// STL
#include <type_traits>

namespace gcep
{
    /*
    * Utility class used to convert gcep::Quaternion to and from JPH::Quat.
    * This class is stateless and must not be instantiated.
    * Jolt dependency is strictly confined to this file.
    */
    class QuaternionConverter
    {
    public:


        // Jolt Conversion

        static JPH::Quat ToJolt(const Quaternion& quaternion)
        {
            /*
            * Jolt expects unit quaternions in most APIs.
            * We normalize explicitly to avoid undefined behavior.
            */
            Quaternion normalized = quaternion.Normalized();

            return JPH::Quat(
                normalized.w,
                normalized.x,
                normalized.y,
                normalized.z
            );
        }

        static Quaternion FromJolt(const JPH::Quat& quaternion)
        {
            return Quaternion(
                quaternion.GetW(),
                quaternion.GetX(),
                quaternion.GetY(),
                quaternion.GetZ()
            );
        }

    private:

        // Creating an instance of this class is prohibited
        QuaternionConverter() = delete;
    };
}