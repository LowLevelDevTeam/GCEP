#pragma once

// STL
#include <cmath>
#include <algorithm>

// Core
#include "vector3.hpp"

namespace gcep
{
    /**
     * @class Quaternion
     * @brief Represents a quaternion used for 3D rotations.
     *
     * This class provides utilities to create, normalize, interpolate,
     * and convert quaternions.
     */
    class Quaternion
    {
    public:
        /** @brief Scalar component of the quaternion. */
        float w;

        /** @brief X component of the vector part. */
        float x;

        /** @brief Y component of the vector part. */
        float y;

        /** @brief Z component of the vector part. */
        float z;

    public:
        /**
         * @brief Constructs the identity quaternion (no rotation).
         *
         * @code
         * Quaternion q;
         * std::cout << "Identity Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: (1, 0, 0, 0)
         * @endcode
         */
        Quaternion();

        /**
         * @brief Constructs a quaternion with the given components.
         * @param w The scalar component.
         * @param x The X component of the vector part.
         * @param y The Y component of the vector part.
         * @param z The Z component of the vector part.
         *
         * @code
         * Quaternion q(0.707f, 0.0f, 0.707f, 0.0f);
         * std::cout << "Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: (0.707, 0.0, 0.707, 0.0)
         * @endcode
         */
        Quaternion(float w, float x, float y, float z);

        /**
         * @brief Destructor.
         */
        ~Quaternion();

        /**
         * @brief Returns the identity quaternion.
         * @return A quaternion representing no rotation.
         *
         * @code
         * Quaternion q = Quaternion::Identity();
         * std::cout << "Identity Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: (1, 0, 0, 0)
         * @endcode
         */
        static Quaternion Identity();

        /**
         * @brief Builds a quaternion from an axis-angle representation.
         * @param axis The axis of rotation (must be normalized).
         * @param angleRadians The rotation angle in radians.
         * @return The resulting quaternion.
         *
         * @code
         * Vector3<float> axis(0.0f, 1.0f, 0.0f);
         * float angle = std::numbers::pi_v<float> / 2.0f;
         * Quaternion q = Quaternion::FromAxisAngle(axis, angle);
         * std::cout << "FromAxisAngle Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: (~=0.707, 0.0, ~=0.707, 0.0)
         * @endcode
         */
        static Quaternion FromAxisAngle(const Vector3<float>& axis, float angleRadians);

        /**
         * @brief Builds a quaternion from Euler angles (in radians).
         * @param eulerRadians A vector containing pitch (X), yaw (Y), and roll (Z).
         * @return The resulting quaternion.
         *
         * The rotation order used is Yaw (Y) * Pitch (X) * Roll (Z).
         *
         * @code
         * Vector3<float> euler(0.0f, std::numbers::pi_v<float> / 4.0f, 0.0f);
         * Quaternion q = Quaternion::FromEuler(euler);
         * std::cout << "FromEuler Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: (~=0.924, 0.0, ~=0.383, 0.0)
         * @endcode
         */
        static Quaternion FromEuler(const Vector3<float>& eulerRadians);

        /**
         * @brief Builds a quaternion that rotates vector "from" to align with vector "to".
         * @param from The starting direction vector.
         * @param to The target direction vector.
         * @return A quaternion representing the rotation from "from" to "to".
         *
         * @code
         * Vector3<float> from(1.0f, 0.0f, 0.0f);
         * Vector3<float> to(0.0f, 0.0f, 1.0f);
         * Quaternion q = Quaternion::FromToRotation(from, to);
         * std::cout << "FromToRotation Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: Quaternion rotating +X to +Z
         * @endcode
         */
        static Quaternion FromToRotation(const Vector3<float>& from, const Vector3<float>& to);

        /**
         * @brief Builds a quaternion that looks in a forward direction with a specified up vector.
         * @param forward The forward direction vector.
         * @param up The up direction vector (default is Y-up).
         * @return A quaternion representing the desired orientation.
         *
         * @code
         * Vector3<float> forward(0.0f, 0.0f, -1.0f);
         * Vector3<float> up(0.0f, 1.0f, 0.0f);
         * Quaternion q = Quaternion::LookRotation(forward, up);
         * std::cout << "LookRotation Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: Quaternion facing forward (-Z)
         * @endcode
         */
        static Quaternion LookRotation(const Vector3<float>& forward, const Vector3<float>& up = { 0.0f, 1.0f, 0.0f });

        /**
         * @brief Returns a normalized copy of this quaternion.
         * @return A unit quaternion representing the same rotation.
         */
        [[nodiscard]] Quaternion Normalized() const;

        /**
         * @brief Normalizes this quaternion in place.
         *
         * @code
         * Quaternion q(2.0f, 0.0f, 0.0f, 0.0f);
         * q.Normalize();
         * std::cout << "Normalized Quaternion: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: (1, 0, 0, 0)
         * @endcode
         */
        void Normalize();

        /**
         * @brief Returns the conjugate of this quaternion.
         * @return The conjugate quaternion.
         *
         * @code
         * Quaternion q(0.707f, 0.0f, 0.707f, 0.0f);
         * Quaternion c = q.Conjugate();
         * std::cout << "Conjugate: (" << c.w << ", " << c.x << ", " << c.y << ", " << c.z << ")\n";
         * // Output: (0.707, -0.0, -0.707, -0.0)
         * @endcode
         */
        [[nodiscard]] Quaternion Conjugate() const;

        /**
         * @brief Returns the inverse of this quaternion.
         * @return The inverse quaternion.
         *
         * @code
         * Quaternion q = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, std::numbers::pi_v<float> / 2.0f);
         * Quaternion inv = q.Inverse();
         * std::cout << "Inverse Quaternion: (" << inv.w << ", " << inv.x << ", " << inv.y << ", " << inv.z << ")\n";
         * // Output: Quaternion representing the opposite rotation
         * @endcode
         */
        [[nodiscard]] Quaternion Inverse() const;

        /**
         * @brief Computes the dot product between two quaternions.
         * @param a The first quaternion.
         * @param b The second quaternion.
         * @return The dot product value.
         *
         * @code
         * Quaternion a(1, 0, 0, 0);
         * Quaternion b(0, 1, 0, 0);
         * float dot = Quaternion::Dot(a, b);
         * std::cout << "Dot Product: " << dot << "\n";
         * // Output: 0
         * @endcode
         */
        static float Dot(const Quaternion& a, const Quaternion& b);

        /**
         * @brief Returns the magnitude (length) of this quaternion.
         * @return The magnitude value.
         */
        [[nodiscard]] float Magnitude() const;

        /**
         * @brief Returns the squared magnitude of this quaternion.
         * @return The squared magnitude value.
         */
        [[nodiscard]] float GetMagnitudeSquared() const;

        /**
         * @brief Computes the angle (in radians) between two quaternions.
         * @param a The first quaternion.
         * @param b The second quaternion.
         * @return The angular difference in radians.
         *
         * @code
         * Quaternion a = Quaternion::FromEuler({ 0.0f, 0.0f, 0.0f });
         * Quaternion b = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f });
         * float angle = Quaternion::Angle(a, b);
         * std::cout << "Angle between rotations: " << angle << " radians\n";
         * // Output: ~=1.5708 radians
         * @endcode
         */
        static float Angle(const Quaternion& a, const Quaternion& b);

        /**
         * @brief Multiplies this quaternion with another (rotation composition).
         * @param other The quaternion to multiply with.
         * @return The resulting quaternion.
         *
         * @code
         * Quaternion q1 = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 4.0f, 0.0f });
         * Quaternion q2 = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 4.0f, 0.0f });
         * Quaternion result = q1 * q2;
         * std::cout << "Combined rotation (Y 90 degrees): (" << result.w << ", " << result.x << ", " << result.y << ", " << result.z << ")\n";
         * // Output: ~=90 degrees rotation around Y
         * @endcode
         */
        Quaternion operator*(const Quaternion& other) const;

        /**
         * @brief Multiplies this quaternion with another and assigns the result.
         * @param other The quaternion to multiply with.
         * @return Reference to this quaternion.
         */
        Quaternion& operator*=(const Quaternion& other);

        /**
         * @brief Checks equality between two quaternions (with tolerance).
         * @param other The quaternion to compare with.
         * @return True if both quaternions are approximately equal.
         *
         * @code
         * Quaternion q1 = Quaternion::Identity();
         * Quaternion q2 = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
         * std::cout << "Equal? " << (q1 == q2 ? "true" : "false") << "\n";
         * // Output: true
         * @endcode
         */
        bool operator==(const Quaternion& other) const;

        /**
         * @brief Checks inequality between two quaternions.
         * @param other The quaternion to compare with.
         * @return True if the quaternions differ.
         *
         * @code
         * Quaternion q1 = Quaternion::FromEuler({ 0.0f, 0.0f, 0.0f });
         * Quaternion q2 = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 4.0f, 0.0f });
         * std::cout << "Different? " << (q1 != q2 ? "true" : "false") << "\n";
         * // Output: true
         * @endcode
         */
        bool operator!=(const Quaternion& other) const;

        /**
         * @brief Rotates a 3D vector by this quaternion.
         * @param v The vector to rotate.
         * @return The rotated vector.
         *
         * @code
         * Quaternion rotation = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 4.0f, 0.0f });
         * Vector3<float> forward(0.0f, 0.0f, -1.0f);
         * Vector3<float> rotated = rotation.RotateVector(forward);
         * std::cout << "Rotated vector: (" << rotated.x << ", " << rotated.y << ", " << rotated.z << ")\n";
         * // Output: Vector diagonally between -Z and -X
         * @endcode
         */
        [[nodiscard]] Vector3<float> RotateVector(const Vector3<float>& v) const;

        /**
         * @brief Performs spherical linear interpolation between two quaternions.
         * @param a The starting quaternion.
         * @param b The target quaternion.
         * @param t Interpolation factor in [0, 1].
         * @return The interpolated quaternion.
         *
         * @code
         * Quaternion a = Quaternion::FromEuler({ 0.0f, 0.0f, 0.0f });
         * Quaternion b = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f });
         * Quaternion q = Quaternion::Slerp(a, b, 0.5f);
         * std::cout << "Slerp halfway rotation: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: ~=45 degrees rotation around Y
         * @endcode
         */
        static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

        /**
         * @brief Performs spherical linear interpolation without clamping t.
         * @param a The starting quaternion.
         * @param b The target quaternion.
         * @param t Interpolation factor (not clamped).
         * @return The interpolated quaternion.
         *
         * @code
         * Quaternion a = Quaternion::FromEuler({ 0.0f, 0.0f, 0.0f });
         * Quaternion b = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f });
         * Quaternion q = Quaternion::SlerpUnclamped(a, b, 1.5f);
         * std::cout << "SlerpUnclamped beyond target: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: ~=135 degrees rotation around Y
         * @endcode
         */
        static Quaternion SlerpUnclamped(const Quaternion& a, const Quaternion& b, float t);

        /**
         * @brief Performs normalized linear interpolation (clamped).
         * @param a The starting quaternion.
         * @param b The target quaternion.
         * @param t Interpolation factor in [0, 1].
         * @return The interpolated quaternion.
         *
         * @code
         * Quaternion a = Quaternion::FromEuler({ 0.0f, 0.0f, 0.0f });
         * Quaternion b = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f });
         * Quaternion q = Quaternion::Lerp(a, b, 0.5f);
         * std::cout << "Lerp halfway rotation: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: ~=45 degrees rotation around Y
         * @endcode
         */
        static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);

        /**
         * @brief Performs normalized linear interpolation without clamping.
         * @param a The starting quaternion.
         * @param b The target quaternion.
         * @param t Interpolation factor (not clamped).
         * @return The interpolated quaternion.
         */
        static Quaternion LerpUnclamped(const Quaternion& a, const Quaternion& b, float t);

        /**
         * @brief Rotates one quaternion toward another by a limited angular step.
         * @param from The starting quaternion.
         * @param to The target quaternion.
         * @param maxRadiansDelta Maximum rotation step in radians.
         * @return The rotated quaternion.
         *
         * @code
         * Quaternion from = Quaternion::FromEuler({ 0.0f, 0.0f, 0.0f });
         * Quaternion to = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f });
         * Quaternion q = Quaternion::RotateTowards(from, to, std::numbers::pi_v<float> / 4.0f);
         * std::cout << "RotateTowards result: (" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")\n";
         * // Output: ~=45 degrees rotation around Y
         * @endcode
         */
        static Quaternion RotateTowards(const Quaternion& from, const Quaternion& to, float maxRadiansDelta);

        /**
         * @brief Extracts the axis and rotation angle (in radians) from this quaternion.
         * @param axis Output parameter for the rotation axis.
         * @param angleRadians Output parameter for the rotation angle in radians.
         *
         * @code
         * Quaternion q = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, std::numbers::pi_v<float> / 3.0f);
         * Vector3<float> axis;
         * float angle;
         * q.ToAxisAngle(axis, angle);
         * std::cout << "Axis: (" << axis.x << ", " << axis.y << ", " << axis.z << "), Angle: " << angle << " radians\n";
         * // Output: Axis (0,1,0), Angle 1.047 rad
         * @endcode
         */
        void ToAxisAngle(Vector3<float>& axis, float& angleRadians) const;

        /**
         * @brief Converts this quaternion to Euler angles (in radians).
         * @return A vector containing pitch (X), yaw (Y), and roll (Z).
         *
         * @code
         * Quaternion q = Quaternion::FromEuler({ 0.0f, std::numbers::pi_v<float> / 4.0f, 0.0f });
         * Vector3<float> euler = q.ToEuler();
         * std::cout << "ToEuler: (" << euler.x << ", " << euler.y << ", " << euler.z << ")\n";
         * // Output: (~=0, ~=0.785, ~=0)
         * @endcode
         */
        [[nodiscard]] Vector3<float> ToEuler() const;
    };
} // gcep
