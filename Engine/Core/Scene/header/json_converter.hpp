#pragma once
#include <Engine/Core/Maths/vector3.hpp>
#include <Engine/Core/Maths/mat3.hpp>
#include <Engine/Core/Maths/mat4.hpp>
#include <Engine/Core/Maths/Quaternion.hpp>
#include <rapidjson.h>

#include "document.h"


namespace gcep::SLS
{
#define REFLECT_COMPONENT(ComponentName, ...) \
    friend struct rfl::reflector<ComponentName>; \
    public: static std::string component_type_name() { return #ComponentName; }

    template <typename T>
    inline Vector2<T> from_json_vec2(const rapidjson::Value& value)
    {
        if (!value.IsArray() || value.Size() != 2) {
            throw std::runtime_error("Expected array of 2 values for Vector2");
        }
        return Vector2<T>(
            value[0].Get<T>(),
            value[1].Get<T>()
        );
    }

    template <typename T, typename Allocator>
    inline rapidjson::Value to_json_vec2(const Vector2<T>& vec, Allocator& allocator)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(vec.x, allocator);
        arr.PushBack(vec.y, allocator);
        return arr;
    }

    template <typename T>
    inline Vector3<T> from_json_vec3(const rapidjson::Value& value)
    {
        if (!value.IsArray() || value.Size() != 3) {
            throw std::runtime_error("Expected array of 3 values for Vector3");
        }
        return Vector3<T>(
            value[0].Get<T>(),
            value[1].Get<T>(),
            value[2].Get<T>()
        );
    }

    template <typename T, typename Allocator>
    inline rapidjson::Value to_json_vec3(const Vector3<T>& vec, Allocator& allocator)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(vec.x, allocator);
        arr.PushBack(vec.y, allocator);
        arr.PushBack(vec.z, allocator);
        return arr;
    }

    template <typename T>
    inline Mat3<T> from_json_Mat3(const rapidjson::Value& value)
    {

        if (!value.IsArray() || value.Size() != 9) {
            throw std::runtime_error("Expected array of 9 values for Mat3");
        }
        return Mat3<T>(
            value[0].Get<T>(), value[1].Get<T>(), value[2].Get<T>(),
            value[3].Get<T>(), value[4].Get<T>(), value[5].Get<T>(),
            value[6].Get<T>(), value[7].Get<T>(), value[8].Get<T>()
        );
    }
    template<typename T, typename Allocator>
    inline rapidjson::Value to_json_Mat3(const Mat3<T>& mat, Allocator& allocator)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                arr.PushBack(mat(i, j), allocator);
            }
        }

        return arr;
    }

    template <typename T>
    inline Mat4<T> from_json_Mat4(const rapidjson::Value& value)
    {
        if (!value.IsArray() || value.Size() != 16) {
            throw std::runtime_error("Expected array of 16 values for Mat4");
        }
        return Mat4<T>(
            value[0].Get<T>(), value[1].Get<T>(), value[2].Get<T>(), value[3].Get<T>(),
            value[4].Get<T>(), value[5].Get<T>(), value[6].Get<T>(), value[7].Get<T>(),
            value[8].Get<T>(), value[9].Get<T>(), value[10].Get<T>(), value[11].Get<T>(),
            value[12].Get<T>(), value[13].Get<T>(), value[14].Get<T>(), value[15].Get<T>()
        );
    }

    template <typename T, typename Allocator>
    inline rapidjson::Value to_json_Mat4(const Mat4<T>& mat, Allocator& allocator)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                arr.PushBack(mat(i, j), allocator);
            }
        }

        return arr;
    }
    inline Quaternion from_json_Quaternion(const rapidjson::Value& value)
    {
        if (!value.IsArray() || value.Size() != 4) {
            throw std::runtime_error("Expected array of 4 values for Quaternion");
        }
        return Quaternion(
            value[0].Get(),
            value[1].Get(),
            value[2].Get(),
            value[3].Get()
        );
    }


    template <typename Allocator>
    inline rapidjson::Value to_json_quaternion(const rapidjson::Value& value, Allocator& allocator)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(value.w, allocator);
        arr.PushBack(value.x, allocator);
        arr.PushBack(value.y, allocator);
        arr.PushBack(value.z, allocator);
        return arr;

    }
}
