#include "Vector3.hpp"

#include <cmath>

namespace gcep
{
    template<Scalar T>
    T Vector3<T>::length() const requires FloatingScalar<T>
    {
        return std::sqrt(std::pow(x, 2) + std::pow(y, 2) + std::pow(z, 2));
    }

    template<Scalar T>
    T Vector3<T>::lengthSquared() const noexcept
    {
        return std::pow(x, 2) + std::pow(y, 2) + std::pow(z, 2);
    }

    template<Scalar T>
    Vector3<T> Vector3<T>::normalized() const requires FloatingScalar<T>
    {
        const T length = length();
        if (length == T(0)) return Vector3();

        return (*this) / length;
    }

    template<Scalar T>
    T Vector3<T>::dot(const Vector3 &vecA, const Vector3 &vecB) noexcept
    {
        return vecA.x * vecB.x + vecA.y * vecB.y + vecA.z * vecB.z;
    }

    template<Scalar T>
    Vector3<T> Vector3<T>::cross(const Vector3 &vecA, const Vector3 &vecB) noexcept
    {
        return Vector3(
            vecA.y * vecB.z - vecA.z * vecB.y,
            vecA.z * vecB.x - vecA.x * vecB.z,
            vecA.x * vecB.y - vecA.y * vecB.x
            );
    }

    template<Scalar T>
    Vector3<T> Vector3<T>::operator+(const Vector3 &other) const noexcept
    {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    template<Scalar T>
    Vector3<T> Vector3<T>::operator-(const Vector3 &other) const noexcept
    {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    template<Scalar T>
    Vector3<T> Vector3<T>::operator*(T scalar) const noexcept
    {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    template<Scalar T>
    Vector3<T> Vector3<T>::operator/(T scalar) const requires FloatingScalar<T>
    {
        return Vector3(x / scalar, y / scalar, z / scalar);
    }
} // gcep