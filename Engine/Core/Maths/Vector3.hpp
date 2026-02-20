#pragma once

// STL
#include <concepts>
#include <type_traits>


namespace gcep
{
    template<typename T>
    concept Scalar = std::is_floating_point_v<T> || (std::is_integral_v<T> && !std::is_same_v<T, bool>);

    template<typename T>
    concept FloatingScalar = std::is_floating_point_v<T>;

    template<Scalar T>
    class Vector3
    {
    public:
        constexpr Vector3() noexcept
            : x(T(0)), y(T(0)), z(T(0))
        {}
        constexpr Vector3(T x_, T y_, T z_) noexcept
            : x(x_), y(y_), z(z_)
        {}

        T length() const requires FloatingScalar<T>;
        T lengthSquared() const noexcept;

        Vector3 normalized() const requires FloatingScalar<T>;

        static T dot(const Vector3& vecA, const Vector3& vecB) noexcept;
        static Vector3 cross(const Vector3& vecA, const Vector3& vecB) noexcept;

        Vector3 operator+(const Vector3& other) const noexcept;
        Vector3 operator-(const Vector3& other) const noexcept;
        Vector3 operator*(T scalar) const noexcept;
        Vector3 operator/(T scalar) const requires FloatingScalar<T>;

    public:
        T x, y, z;
    };
} // gcep

#include "Vector3.inl"
