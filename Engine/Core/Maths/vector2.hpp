#pragma once

#include <cmath>
#include <numbers>

namespace gcep
{
    /**
	 * @class Vector2
     * @brief Represents a 2D mathematical vector with common arithmetic operations.
     *
     * This class provides a complete set of vector math operations such as addition,
     * subtraction, normalization, dot product, distance computation, and interpolation.
     *
     * @tparam T Numeric type (float, double, int, etc.)
     *
     * @code
     * math::Vector2<float> a(3.0f, 4.0f);
     * math::Vector2<float> b(1.0f, 2.0f);
     * float distance = math::Vector2<float>::Distance(a, b);
     * math::Vector2<float> normalized = a.Normalized();
     * @endcode
     */
    template<typename T>
    class Vector2
    {
    public:
        /** @brief X component of the vector. */
        T x;

        /** @brief Y component of the vector. */
        T y;

    public:
        // ======================
        // Public Constructors
        // ======================

        /** @brief Default constructor. Initializes components to zero. */
        constexpr Vector2();

        /** @brief Constructs a vector from given components. */
        constexpr Vector2(T x_, T y_);

        /** @brief Copy constructor with type conversion. */
        template<typename U>
        constexpr Vector2(const Vector2<U>& other);

        // ======================
        // Public Arithmetic Operators
        // ======================

        /** @brief Adds two vectors component-wise. */
        constexpr Vector2 operator+(const Vector2& rhs) const;

        /** @brief Subtracts two vectors component-wise. */
        constexpr Vector2 operator-(const Vector2& rhs) const;

        /** @brief Adds another vector to this one. */
        constexpr Vector2& operator+=(const Vector2& rhs);

        /** @brief Subtracts another vector from this one. */
        constexpr Vector2& operator-=(const Vector2& rhs);

        /** @brief Multiplies this vector by a scalar. */
        constexpr Vector2 operator*(T scalar) const;

        /** @brief Divides this vector by a scalar. */
        constexpr Vector2 operator/(T scalar) const;

        /** @brief Multiplies this vector by a scalar (in-place). */
        constexpr Vector2& operator*=(T scalar);

        /** @brief Divides this vector by a scalar (in-place). */
        constexpr Vector2& operator/=(T scalar);

        // ======================
        // Public Comparison
        // ======================

        /**
         * @brief Checks approximate equality between two vectors.
         * @param rhs The vector to compare against.
         * @param epsilon Tolerance threshold for comparison.
         */
        constexpr bool Equals(const Vector2& rhs, T epsilon = static_cast<T>(1e-6)) const;

        /** @brief Checks for exact equality between two vectors. */
        constexpr bool operator==(const Vector2& rhs) const;

        /** @brief Checks for inequality between two vectors. */
        constexpr bool operator!=(const Vector2& rhs) const;

        // ======================
        // Public Vector Math
        // ======================

        /** @brief Computes the dot product between this and another vector. */
        constexpr T Dot(const Vector2& rhs) const;

        /** @brief Returns the Euclidean length (magnitude) of this vector. */
        T Length() const;

        /** @brief Returns the squared length of this vector (avoids sqrt). */
        T LengthSquared() const;

        /** @brief Returns a normalized copy of this vector. */
        Vector2 Normalized() const;

        /** @brief Normalizes this vector in-place. */
        void Normalize();

        /** @brief Computes the distance between two vectors. */
        static T Distance(const Vector2& a, const Vector2& b);

        /** @brief Linearly interpolates between two vectors. */
        static Vector2 Lerp(const Vector2& a, const Vector2& b, T t);

        /**
         * @brief Computes the angle (in degrees) between two vectors.
         *
         * @note The result is always between 0 degree and 180 degrees.
         */
        static T Angle(const Vector2& a, const Vector2& b);

        /** @brief Returns a perpendicular vector rotated 90 degrees counter-clockwise. */
        Vector2 Perpendicular() const;

        /** @brief Performs component-wise scaling between two vectors. */
        static Vector2 Scale(const Vector2& a, const Vector2& b);

        /** @brief Returns a vector containing the component-wise minima. */
        static Vector2 Min(const Vector2& a, const Vector2& b);

        /** @brief Returns a vector containing the component-wise maxima. */
        static Vector2 Max(const Vector2& a, const Vector2& b);

        /**
         * @brief Moves a point towards a target by a fixed distance delta.
         *
         * @param current The current vector position.
         * @param target The destination vector.
         * @param maxDistanceDelta The maximum movement allowed.
         * @return The new position vector.
         */
        static Vector2 MoveTowards(const Vector2& current, const Vector2& target, T maxDistanceDelta);
    };
} // gcep

#include "vector2.inl"
