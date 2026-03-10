#include "quaternion.hpp"

namespace gcep
{
    Quaternion::Quaternion()
        : w(1.0f), x(0.0f), y(0.0f), z(0.0f)
    {}

    Quaternion::Quaternion(float w, float x, float y, float z)
        : w(w), x(x), y(y), z(z)
    {}

    Quaternion::~Quaternion() = default;

    Quaternion Quaternion::Identity()
    {
        return {1.0f, 0.0f, 0.0f, 0.0f};
    }

    Quaternion Quaternion::FromAxisAngle(const Vector3<float>& axis, float angleRadians)
    {
        Vector3<float> normAxis = axis.Normalized();

        float halfAngle = angleRadians * 0.5f;

        float s = std::sin(halfAngle);
        float c = std::cos(halfAngle);

        return {c, normAxis.x * s, normAxis.y * s, normAxis.z * s};
    }

    Quaternion Quaternion::FromEuler(const Vector3<float>& euler)
    {
        float cx = std::cos(euler.x * 0.5f);
        float sx = std::sin(euler.x * 0.5f);
        float cy = std::cos(euler.y * 0.5f);
        float sy = std::sin(euler.y * 0.5f);
        float cz = std::cos(euler.z * 0.5f);
        float sz = std::sin(euler.z * 0.5f);

        Quaternion q;
        q.w = cx * cy * cz + sx * sy * sz;
        q.x = sx * cy * cz - cx * sy * sz;
        q.y = cx * sy * cz + sx * cy * sz;
        q.z = cx * cy * sz - sx * sy * cz;

        return q;
    }

    Quaternion Quaternion::FromToRotation(const Vector3<float>& from, const Vector3<float>& to)
    {
        Vector3<float> f = from.Normalized();
        Vector3<float> t = to.Normalized();

        float dot = f.Dot(t);
        dot = std::clamp(dot, -1.0f, 1.0f);

        if (dot > 0.9999f) return Identity();
        if (dot < -0.9999f)
        {
            Vector3<float> axis = std::fabs(f.x) > std::fabs(f.z)
                ? Vector3<float>(-f.y, f.x, 0.0f)
                : Vector3<float>(0.0f, -f.z, f.y);

            return FromAxisAngle(axis, 3.1415926535f); // Pi rad -> 180�
        }

        Vector3<float> cross =
        {
            f.y * t.z - f.z * t.y,
            f.z * t.x - f.x * t.z,
            f.x * t.y - f.y * t.x
        };

        float sqrt = std::sqrt((1 + dot) * 2);
        float invSqrt = 1.0f / sqrt;

        return {sqrt * 0.5f, cross.x * invSqrt, cross.y * invSqrt, cross.z * invSqrt};
    }

    Quaternion Quaternion::LookRotation(const Vector3<float>& forward, const Vector3<float>& up)
    {
        Vector3<float> f = forward.Normalized();
        Vector3<float> r = up.Normalized().Cross(f).Normalized();
        Vector3<float> u = f.Cross(r);

        float m00 = r.x, m01 = u.x, m02 = f.x;
        float m10 = r.y, m11 = u.y, m12 = f.y;
        float m20 = r.z, m21 = u.z, m22 = f.z;

        float trace = m00 + m11 + m22;

        Quaternion q;
        if (trace > 0.0f)
        {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        }
        else if (m00 > m11 && m00 > m22)
        {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        }
        else if (m11 > m22)
        {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        }
        else
        {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        return q.Normalized();
    }

    Quaternion Quaternion::Normalized() const
    {
        float magSq = GetMagnitudeSquared();
        if (magSq == 0.0f) return Identity();

        float inv = 1.0f / std::sqrt(magSq);

        return { w * inv, x * inv, y * inv, z * inv };
    }

    void Quaternion::Normalize()
    {
        float magSq = GetMagnitudeSquared();

        if (magSq == 0.0f)
        {
            *this = Identity();
            return;
        }

        float inv = 1.0f / std::sqrt(magSq);
        w *= inv; x *= inv; y *= inv; z *= inv;
    }

    Quaternion Quaternion::Conjugate() const { return { w, -x, -y, -z }; }

    Quaternion Quaternion::Inverse() const
    {
        float magSq = GetMagnitudeSquared();
        if (magSq == 0.0f) return Identity();

        Quaternion qc = Conjugate();

        float invMagSq = 1.0f / magSq;

        return { qc.w * invMagSq, qc.x * invMagSq, qc.y * invMagSq, qc.z * invMagSq };
    }

    float Quaternion::Dot(const Quaternion& a, const Quaternion& b)
    {
        return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float Quaternion::Magnitude() const { return std::sqrt(GetMagnitudeSquared()); }

    float Quaternion::GetMagnitudeSquared() const { return w * w + x * x + y * y + z * z; }

    float Quaternion::Angle(const Quaternion& a, const Quaternion& b)
    {
        float dot = std::clamp(Dot(a.Normalized(), b.Normalized()), -1.0f, 1.0f);
        return std::acos(dot) * 2.0f;
    }

    Quaternion Quaternion::operator*(const Quaternion& o) const
    {
        return
        {
            w * o.w - x * o.x - y * o.y - z * o.z,
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w
        };
    }

    Quaternion& Quaternion::operator*=(const Quaternion& o)
    {
        *this = *this * o;
        return *this;
    }

    bool Quaternion::operator==(const Quaternion& o) const
    {
        return std::fabs(w - o.w) < 1e-6f &&
            std::fabs(x - o.x) < 1e-6f &&
            std::fabs(y - o.y) < 1e-6f &&
            std::fabs(z - o.z) < 1e-6f;
    }

    bool Quaternion::operator!=(const Quaternion& o) const { return !(*this == o); }

    Vector3<float> Quaternion::RotateVector(const Vector3<float>& v) const
    {
        Quaternion qv(0.0f, v.x, v.y, v.z);
        Quaternion res = (*this) * qv * Conjugate();
        return { res.x, res.y, res.z };
    }

    Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        return SlerpUnclamped(a, b, std::clamp(t, 0.0f, 1.0f));
    }

    Quaternion Quaternion::SlerpUnclamped(const Quaternion& a, const Quaternion& b, float t)
    {
        float dot = Dot(a, b);
        Quaternion bCopy = b;
        if (dot < 0.0f)
        {
            bCopy = { -b.w, -b.x, -b.y, -b.z };
            dot = -dot;
        }

        const float THRESH = 0.9995f;
        if (dot > THRESH)
        {
            Quaternion r(
                a.w + t * (bCopy.w - a.w),
                a.x + t * (bCopy.x - a.x),
                a.y + t * (bCopy.y - a.y),
                a.z + t * (bCopy.z - a.z)
            );
            r.Normalize();
            return r;
        }

        float theta0 = std::acos(dot);
        float theta = theta0 * t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);
        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;

        Quaternion r(
            (a.w * s0) + (bCopy.w * s1),
            (a.x * s0) + (bCopy.x * s1),
            (a.y * s0) + (bCopy.y * s1),
            (a.z * s0) + (bCopy.z * s1)
        );
        r.Normalize();
        return r;
    }

    Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float t)
    {
        return LerpUnclamped(a, b, std::clamp(t, 0.0f, 1.0f));
    }

    Quaternion Quaternion::LerpUnclamped(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion q(
            a.w + t * (b.w - a.w),
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z)
        );

        q.Normalize();

        return q;
    }

    Quaternion Quaternion::RotateTowards(const Quaternion& from, const Quaternion& to, float maxRadiansDelta)
    {
        float angle = Angle(from, to);
        if (angle == 0.0f) return to;

        float t = std::min(1.0f, maxRadiansDelta / angle);

        return SlerpUnclamped(from, to, t);
    }

    void Quaternion::ToAxisAngle(Vector3<float>& axis, float& angleRadians) const
    {
        Quaternion q = Normalized();
        angleRadians = 2.0f * std::acos(q.w);

        float s = std::sqrt(1.0f - q.w * q.w);
        if (s < 1e-6f)
            axis = {1.0f, 0.0f, 0.0f};
        else
            axis = {q.x / s, q.y / s, q.z / s};
    }

    Vector3<float> Quaternion::ToEuler() const
    {
        Vector3<float> euler;

        // Pitch (X-axis rotation)
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        euler.x = std::atan2(sinr_cosp, cosr_cosp);

        // Yaw (Y-axis rotation)
        float sinp = 2.0f * (w * y - z * x);
        if (std::fabs(sinp) >= 1)
            euler.y = std::copysign(3.1415926535f / 2.0f, sinp);
        else
            euler.y = std::asin(sinp);

        // Roll (Z-axis rotation)
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        euler.z = std::atan2(siny_cosp, cosy_cosp);

        return euler;
    }
} // gcep
