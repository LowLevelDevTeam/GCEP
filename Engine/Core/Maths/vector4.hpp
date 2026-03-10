#pragma once

#include <cmath>
#include <numbers>


namespace gcep::mth {

            /**
     * @class Vector4
     * @brief Represents a 4D mathematical vector with common arithmetic operations.
     *
     * Provides a complete set of vector operations such as addition, subtraction,
     * normalization, dot product, interpolation, and reflection.
     * Commonly used for homogeneous coordinates (XYZW) in 3D graphics.
     *
     * @tparam T Numeric type (float, double, int, etc.)
     *
     * @code
     * math::Vector4<float> a(1.0f, 0.0f, 0.0f, 1.0f);
     * math::Vector4<float> b(0.0f, 1.0f, 0.0f, 1.0f);
     * float dot = a.Dot(b); // 0.0f
     * @endcode
     */
    template<typename T>
    class Vector4
    {
    public:
        /** @brief X component of the vector. */
        T x;

        /** @brief Y component of the vector. */
        T y;

        /** @brief Z component of the vector. */
        T z;

        /** @brief W component of the vector. */
        T w;

    public:

        // ======================
        // Public Constructors
        // ======================

        /** @brief Default constructor. Initializes all components to zero. */
        constexpr Vector4() : x{}, y{}, z{}, w{} {}

        /** @brief Constructs a vector with specific x, y, z, w components. */
        constexpr Vector4(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}

        /** @brief Copy constructor allowing conversion between types. */
        template<typename U>
        constexpr Vector4(const Vector4<U>& other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)), w(static_cast<T>(other.w)) {}

        // ======================
        // Public Arithmetic Operators
        // ======================

        /** @brief Adds two vectors component-wise. */
        constexpr Vector4 operator+(const Vector4& rhs) const;

        /** @brief Subtracts two vectors component-wise. */
        constexpr Vector4 operator-(const Vector4& rhs) const;

        /** @brief Adds another vector to this one (in-place). */
        constexpr Vector4& operator+=(const Vector4& rhs);

        /** @brief Subtracts another vector from this one (in-place). */
        constexpr Vector4& operator-=(const Vector4& rhs);

        /** @brief Multiplies the vector by a scalar. */
        constexpr Vector4 operator*(T scalar) const;

        /** @brief Divides the vector by a scalar. */
        constexpr Vector4 operator/(T scalar) const;

        /** @brief Multiplies the vector by a scalar (in-place). */
        constexpr Vector4& operator*=(T scalar);

        /** @brief Divides the vector by a scalar (in-place). */
        constexpr Vector4& operator/=(T scalar);

        // ======================
        // Public Comparison
        // ======================

        /**
         * @brief Checks approximate equality between two vectors.
         * @param rhs Vector to compare against.
         * @param epsilon Allowed numerical tolerance.
         * @return True if both vectors are approximately equal.
         */
        constexpr bool Equals(const Vector4& rhs, T epsilon = static_cast<T>(1e-6)) const;

        /** @brief Checks exact equality between two vectors. */
        constexpr bool operator==(const Vector4& rhs) const;

        /** @brief Checks inequality between two vectors. */
        constexpr bool operator!=(const Vector4& rhs) const;

        // ======================
        // Public Vector Math
        // ======================

        /** @brief Computes the dot product between this vector and another. */
        constexpr T Dot(const Vector4& rhs) const;

        /** @brief Returns the magnitude (length) of the vector. */
        T Length() const;

        /** @brief Returns the squared magnitude (avoids using sqrt). */
        T LengthSquared() const;

        /** @brief Returns a normalized (unit length) copy of the vector. */
        Vector4 Normalized() const;

        /** @brief Normalizes this vector in-place. */
        void Normalize();

        /** @brief Computes the Euclidean distance between two vectors. */
        static T Distance(const Vector4& a, const Vector4& b);

        /** @brief Linearly interpolates between two vectors. */
        static Vector4 Lerp(const Vector4& a, const Vector4& b, T t);

        /** @brief Performs component-wise scaling between two vectors. */
        static Vector4 Scale(const Vector4& a, const Vector4& b);

        /** @brief Returns a vector containing the component-wise minima. */
        static Vector4 Min(const Vector4& a, const Vector4& b);

        /** @brief Returns a vector containing the component-wise maxima. */
        static Vector4 Max(const Vector4& a, const Vector4& b);

        /**
         * @brief Moves a point towards a target by a fixed distance delta.
         *
         * @param current Current position.
         * @param target Target position.
         * @param maxDistanceDelta Maximum movement allowed.
         * @return New position vector after movement.
         */
        static Vector4 MoveTowards(const Vector4& current, const Vector4& target, T maxDistanceDelta);

        /**
         * @brief Reflects a vector around a normal.
         *
         * @param direction Incident direction vector.
         * @param normal Normal vector to reflect around (should be normalized).
         * @return Reflected vector.
         */
        static Vector4 Reflect(const Vector4& direction, const Vector4& normal);

        /**
         * @brief Computes the angle between two vectors in degrees.
         *
         * The result is always between 0 and 180 degrees.
         */
        static T Angle(const Vector4& a, const Vector4& b);
    };

}

