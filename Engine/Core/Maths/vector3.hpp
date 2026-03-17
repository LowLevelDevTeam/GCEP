#pragma once

// STL
#include <cmath>
#include <numbers>

namespace gcep
{
    /**
	 * @class Vector3
     * @brief Represents a 3D mathematical vector with common arithmetic operations.
     *
     * Provides a complete set of vector operations such as addition, subtraction,
     * normalization, dot and cross product, interpolation, and reflection.
     *
     * @tparam T Numeric type (float, double, int, etc.)
     *
     * @code
     * math::Vector3<float> a(1.0f, 0.0f, 0.0f);
     * math::Vector3<float> b(0.0f, 1.0f, 0.0f);
     * float dot = a.Dot(b); // 0.0f
     * math::Vector3<float> cross = a.Cross(b); // (0, 0, 1)
     * @endcode
     */
    template<typename T>
    class Vector3
    {
    public:
        /** @brief X component of the vector. */
        T x;

        /** @brief Y component of the vector. */
        T y;

        /** @brief Z component of the vector. */
        T z;

    public:

        // ======================
        // Public Constructors
        // ======================

        /** @brief Default constructor. Initializes all components to zero. */
        constexpr Vector3();

        /** @brief Constructs a vector with specific x, y, z components. */
        constexpr Vector3(T x_, T y_, T z_);

        /** @brief Copy constructor allowing conversion between types. */
        template<typename U>
        constexpr Vector3(const Vector3<U>& other);

        // ======================
        // Public Arithmetic Operators
        // ======================

        /** @brief Adds two vectors component-wise. */
        constexpr Vector3 operator+(const Vector3& rhs) const;

        /** @brief Subtracts two vectors component-wise. */
        constexpr Vector3 operator-(const Vector3& rhs) const;

        /** @brief Adds another vector to this one (in-place). */
        constexpr Vector3& operator+=(const Vector3& rhs);

        /** @brief Subtracts another vector from this one (in-place). */
        constexpr Vector3& operator-=(const Vector3& rhs);

        /** @brief Multiplies the vector by a scalar. */
        constexpr Vector3 operator*(T scalar) const;

        /** @brief Divides the vector by a scalar. */
        constexpr Vector3 operator/(T scalar) const;

        /** @brief Multiplies the vector by a scalar (in-place). */
        constexpr Vector3& operator*=(T scalar);

        /** @brief Divides the vector by a scalar (in-place). */
        constexpr Vector3& operator/=(T scalar);

        // ======================
        // Public Comparison
        // ======================

        /**
         * @brief Checks approximate equality between two vectors.
         * @param rhs Vector to compare against.
         * @param epsilon Allowed numerical tolerance.
         * @return True if both vectors are approximately equal.
         */
        constexpr bool Equals(const Vector3& rhs, T epsilon = static_cast<T>(1e-6)) const;

        /** @brief Checks exact equality between two vectors. */
        constexpr bool operator==(const Vector3& rhs) const;

        /** @brief Checks inequality between two vectors. */
        constexpr bool operator!=(const Vector3& rhs) const;

        // ======================
        // Public Vector Math
        // ======================

        /** @brief Computes the dot product between this vector and another. */
        constexpr T Dot(const Vector3& rhs) const;

        /** @brief Computes the cross product between this vector and another. */
        constexpr Vector3 Cross(const Vector3& rhs) const;

        /** @brief Returns the magnitude (length) of the vector. */
        T Length() const;

        /** @brief Returns the squared magnitude (avoids using sqrt). */
        T LengthSquared() const;

        /** @brief Returns a normalized (unit length) copy of the vector. */
        Vector3 Normalized() const;

        /** @brief Normalizes this vector in-place. */
        void Normalize();

        /** @brief Computes the Euclidean distance between two vectors. */
        static T Distance(const Vector3& a, const Vector3& b);

        /** @brief Linearly interpolates between two vectors. */
        static Vector3 Lerp(const Vector3& a, const Vector3& b, T t);

        /**
         * @brief Returns a perpendicular vector.
         *
         * The resulting vector is arbitrary but guaranteed to be orthogonal
         * to the current vector. Useful for generating tangent or normal bases.
         */
        Vector3 Perpendicular() const;

        /** @brief Performs component-wise scaling between two vectors. */
        static Vector3 Scale(const Vector3& a, const Vector3& b);

        /** @brief Returns a vector containing the component-wise minima. */
        static Vector3 Min(const Vector3& a, const Vector3& b);

        /** @brief Returns a vector containing the component-wise maxima. */
        static Vector3 Max(const Vector3& a, const Vector3& b);

        /**
         * @brief Moves a point towards a target by a fixed distance delta.
         *
         * @param current Current position.
         * @param target Target position.
         * @param maxDistanceDelta Maximum movement allowed.
         * @return New position vector after movement.
         */
        static Vector3 MoveTowards(const Vector3& current, const Vector3& target, T maxDistanceDelta);

        /**
         * @brief Reflects a vector around a normal.
         *
         * @param direction Incident direction vector.
         * @param normal Normal vector to reflect around (should be normalized).
         * @return Reflected vector.
         */
        static Vector3 Reflect(const Vector3& direction, const Vector3& normal);

        /**
         * @brief Computes the angle between two vectors in degrees.
         *
         * The result is always between 0 degree and 180 degrees.
         */
        static T Angle(const Vector3& a, const Vector3& b);
    };
} // namespace gcep

#include "vector3.inl"
