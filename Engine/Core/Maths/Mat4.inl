// STL
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace gcep
{

    // Constructors
    template <typename T>
    Mat4<T>::Mat4()
    {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                m[i][j] = (i == j) ? static_cast<T>(1) : static_cast<T>(0);
    }

    template <typename T>
    Mat4<T>::Mat4(T m00, T m01, T m02, T m03,
        T m10, T m11, T m12, T m13,
        T m20, T m21, T m22, T m23,
        T m30, T m31, T m32, T m33)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
    }

    template <typename T>
    Mat4<T>::~Mat4() = default;

    // Static Methods
    template <typename T>
    Mat4<T> Mat4<T>::Identity()
    {
        Mat4<T> result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = (i == j) ? static_cast<T>(1) : static_cast<T>(0);
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Zero()
    {
        Mat4<T> result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = static_cast<T>(0);
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::TRS(const Vector3<T>& position, const Quaternion& rotation, const Vector3<T>& scale)
    {
        Mat4<T> rot = Rotate(rotation);
        Mat4<T> scl = Scale(scale);
        Mat4<T> trs = rot * scl;
        trs.m[3][0] = position.x;
        trs.m[3][1] = position.y;
        trs.m[3][2] = position.z;
        trs.m[3][3] = static_cast<T>(1);
        return trs;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Perspective(T fovYRadians, T aspect, T nearZ, T farZ)
    {
        T f = static_cast<T>(1) / std::tan(fovYRadians / static_cast<T>(2));
        Mat4<T> result = Zero();

        result.m[0][0] = f / aspect;
        result.m[1][1] = f;
        result.m[2][2] = (farZ + nearZ) / (nearZ - farZ);
        result.m[2][3] = (static_cast<T>(2) * farZ * nearZ) / (nearZ - farZ);
        result.m[3][2] = static_cast<T>(-1);

        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Ortho(T left, T right, T bottom, T top, T nearZ, T farZ)
    {
        Mat4<T> result = Identity();
        result.m[0][0] = static_cast<T>(2) / (right - left);
        result.m[1][1] = static_cast<T>(2) / (top - bottom);
        result.m[2][2] = static_cast<T>(-2) / (farZ - nearZ);
        result.m[3][0] = -(right + left) / (right - left);
        result.m[3][1] = -(top + bottom) / (top - bottom);
        result.m[3][2] = -(farZ + nearZ) / (farZ - nearZ);
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::LookAt(const Vector3<T>& eye, const Vector3<T>& target, const Vector3<T>& up)
    {
        Vector3<T> zAxis = (eye - target).Normalized();
        Vector3<T> xAxis = up.Cross(zAxis).Normalized();
        Vector3<T> yAxis = zAxis.Cross(xAxis);

        Mat4<T> result = Identity();
        result.m[0][0] = xAxis.x; result.m[0][1] = yAxis.x; result.m[0][2] = zAxis.x;
        result.m[1][0] = xAxis.y; result.m[1][1] = yAxis.y; result.m[1][2] = zAxis.y;
        result.m[2][0] = xAxis.z; result.m[2][1] = yAxis.z; result.m[2][2] = zAxis.z;
        result.m[3][0] = -xAxis.Dot(eye);
        result.m[3][1] = -yAxis.Dot(eye);
        result.m[3][2] = -zAxis.Dot(eye);
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Translate(const Vector3<T>& position)
    {
        Mat4<T> result = Identity();
        result.m[3][0] = position.x;
        result.m[3][1] = position.y;
        result.m[3][2] = position.z;
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Scale(const Vector3<T>& scale)
    {
        Mat4<T> result = Identity();
        result.m[0][0] = scale.x;
        result.m[1][1] = scale.y;
        result.m[2][2] = scale.z;
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Rotate(const Quaternion& rotation)
    {
        Mat4<T> result = Identity();
        T xx = static_cast<T>(rotation.x) * static_cast<T>(rotation.x);
        T yy = static_cast<T>(rotation.y) * static_cast<T>(rotation.y);
        T zz = static_cast<T>(rotation.z) * static_cast<T>(rotation.z);
        T xy = static_cast<T>(rotation.x) * static_cast<T>(rotation.y);
        T xz = static_cast<T>(rotation.x) * static_cast<T>(rotation.z);
        T yz = static_cast<T>(rotation.y) * static_cast<T>(rotation.z);
        T wx = static_cast<T>(rotation.w) * static_cast<T>(rotation.x);
        T wy = static_cast<T>(rotation.w) * static_cast<T>(rotation.y);
        T wz = static_cast<T>(rotation.w) * static_cast<T>(rotation.z);

        result.m[0][0] = static_cast<T>(1) - static_cast<T>(2) * (yy + zz);
        result.m[0][1] = static_cast<T>(2) * (xy + wz);
        result.m[0][2] = static_cast<T>(2) * (xz - wy);
        result.m[1][0] = static_cast<T>(2) * (xy - wz);
        result.m[1][1] = static_cast<T>(1) - static_cast<T>(2) * (xx + zz);
        result.m[1][2] = static_cast<T>(2) * (yz + wx);
        result.m[2][0] = static_cast<T>(2) * (xz + wy);
        result.m[2][1] = static_cast<T>(2) * (yz - wx);
        result.m[2][2] = static_cast<T>(1) - static_cast<T>(2) * (xx + yy);
        return result;
    }

    // Operations
    template <typename T>
    Mat4<T> Mat4<T>::Transpose() const
    {
        Mat4<T> result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = m[j][i];
        return result;
    }

    template <typename T>
    Mat4<T> Mat4<T>::Inverse() const
    {
        T inv[16], det;
        const T* m = &this->m[0][0];

        inv[0] = m[5] * m[10] * m[15] -
            m[5] * m[11] * m[14] -
            m[9] * m[6] * m[15] +
            m[9] * m[7] * m[14] +
            m[13] * m[6] * m[11] -
            m[13] * m[7] * m[10];

        inv[4] = -m[4] * m[10] * m[15] +
            m[4] * m[11] * m[14] +
            m[8] * m[6] * m[15] -
            m[8] * m[7] * m[14] -
            m[12] * m[6] * m[11] +
            m[12] * m[7] * m[10];

        inv[8] = m[4] * m[9] * m[15] -
            m[4] * m[11] * m[13] -
            m[8] * m[5] * m[15] +
            m[8] * m[7] * m[13] +
            m[12] * m[5] * m[11] -
            m[12] * m[7] * m[9];

        inv[12] = -m[4] * m[9] * m[14] +
            m[4] * m[10] * m[13] +
            m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] -
            m[12] * m[5] * m[10] +
            m[12] * m[6] * m[9];

        inv[1] = -m[1] * m[10] * m[15] +
            m[1] * m[11] * m[14] +
            m[9] * m[2] * m[15] -
            m[9] * m[3] * m[14] -
            m[13] * m[2] * m[11] +
            m[13] * m[3] * m[10];

        inv[5] = m[0] * m[10] * m[15] -
            m[0] * m[11] * m[14] -
            m[8] * m[2] * m[15] +
            m[8] * m[3] * m[14] +
            m[12] * m[2] * m[11] -
            m[12] * m[3] * m[10];

        inv[9] = -m[0] * m[9] * m[15] +
            m[0] * m[11] * m[13] +
            m[8] * m[1] * m[15] -
            m[8] * m[3] * m[13] -
            m[12] * m[1] * m[11] +
            m[12] * m[3] * m[9];

        inv[13] = m[0] * m[9] * m[14] -
            m[0] * m[10] * m[13] -
            m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] +
            m[12] * m[1] * m[10] -
            m[12] * m[2] * m[9];

        inv[2] = m[1] * m[6] * m[15] -
            m[1] * m[7] * m[14] -
            m[5] * m[2] * m[15] +
            m[5] * m[3] * m[14] +
            m[13] * m[2] * m[7] -
            m[13] * m[3] * m[6];

        inv[6] = -m[0] * m[6] * m[15] +
            m[0] * m[7] * m[14] +
            m[4] * m[2] * m[15] -
            m[4] * m[3] * m[14] -
            m[12] * m[2] * m[7] +
            m[12] * m[3] * m[6];

        inv[10] = m[0] * m[5] * m[15] -
            m[0] * m[7] * m[13] -
            m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] -
            m[12] * m[3] * m[5];

        inv[14] = -m[0] * m[5] * m[14] +
            m[0] * m[6] * m[13] +
            m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] +
            m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] +
            m[1] * m[7] * m[10] +
            m[5] * m[2] * m[11] -
            m[5] * m[3] * m[10] -
            m[9] * m[2] * m[7] +
            m[9] * m[3] * m[6];

        inv[7] = m[0] * m[6] * m[11] -
            m[0] * m[7] * m[10] -
            m[4] * m[2] * m[11] +
            m[4] * m[3] * m[10] +
            m[8] * m[2] * m[7] -
            m[8] * m[3] * m[6];

        inv[11] = -m[0] * m[5] * m[11] +
            m[0] * m[7] * m[9] +
            m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] +
            m[8] * m[3] * m[5];

        inv[15] = m[0] * m[5] * m[10] -
            m[0] * m[6] * m[9] -
            m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] -
            m[8] * m[2] * m[5];

        det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        if (det == 0)
            throw std::runtime_error("Matrix is singular and cannot be inverted.");

        det = 1.0 / det;

        Mat4<T> result;
        for (int i = 0; i < 16; i++)
            reinterpret_cast<T*>(result.m)[i] = inv[i] * det;

        return result;
    }

    template <typename T>
    T& Mat4<T>::operator()(int row, int col)
    {
        return m[row][col];
    }

    template <typename T>
    const T& Mat4<T>::operator()(int row, int col) const
    {
        return m[row][col];
    }

    template <typename T>
    Vector3<T> Mat4<T>::ExtractPosition() const
    {
        return Vector3<T>(m[3][0], m[3][1], m[3][2]);
    }

    template <typename T>
    Vector3<T> Mat4<T>::ExtractScale() const
    {
        Vector3<T> scale;
        scale.x = Vector3<T>(m[0][0], m[0][1], m[0][2]).Length();
        scale.y = Vector3<T>(m[1][0], m[1][1], m[1][2]).Length();
        scale.z = Vector3<T>(m[2][0], m[2][1], m[2][2]).Length();
        return scale;
    }

    template <typename T>
    Vector3<T> Mat4<T>::GetRow(int row) const
    {
        return Vector3<T>(
            m[row][0],
            m[row][1],
            m[row][2]
        );
    }

    template <typename T>
    Vector3<T> Mat4<T>::GetColumn(int col) const
    {
        return Vector3<T>(
            m[0][col],
            m[1][col],
            m[2][col]
        );
    }

    template <typename T>
    Mat4<T> Mat4<T>::operator*(const Mat4<T>& other) const
    {
        Mat4<T> result = Mat4<T>::Zero();
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                T sum = static_cast<T>(0);
                for (int k = 0; k < 4; ++k)
                    sum += m[i][k] * other.m[k][j];
                result.m[i][j] = sum;
            }
        }
        return result;
    }

    template <typename T>
    Vector3<T> Mat4<T>::MultiplyPoint(const Vector3<T>& v) const
    {
        T x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0];
        T y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1];
        T z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2];
        T w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + m[3][3];

        if (w != static_cast<T>(0) && w != static_cast<T>(1))
        {
            x /= w;
            y /= w;
            z /= w;
        }

        return Vector3<T>(x, y, z);
    }

    template <typename T>
    Vector3<T> Mat4<T>::MultiplyVector(const Vector3<T>& v) const
    {
        T x = v.x * m[0][0] + v.y * m[0][1] + v.z * m[0][2];
        T y = v.x * m[1][0] + v.y * m[1][1] + v.z * m[1][2];
        T z = v.x * m[2][0] + v.y * m[2][1] + v.z * m[2][2];
        return Vector3<T>(x, y, z);
    }

    template <typename T>
    T Mat4<T>::Determinant() const
    {
        // Compute determinant via expansion by minors (3x3 determinants).
        auto det3 = [](T a1, T a2, T a3,
            T b1, T b2, T b3,
            T c1, T c2, T c3) -> T
            {
                return a1 * (b2 * c3 - b3 * c2)
                    - a2 * (b1 * c3 - b3 * c1)
                    + a3 * (b1 * c2 - b2 * c1);
            };

        T det =
            m[0][0] * det3(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3])
            - m[0][1] * det3(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3])
            + m[0][2] * det3(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3])
            - m[0][3] * det3(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);

        return det;
    }

    template <typename T>
    Quaternion Mat4<T>::ExtractRotation() const
    {
        // Convert upper-left 3x3 to quaternion using standard matrix-to-quaternion conversion.
        // Assumes matrix is rotation (possibly with scale). We remove scale first.
        Vector3<T> scale = ExtractScale();
        T sx = scale.x, sy = scale.y, sz = scale.z;
        T invSx = (sx != static_cast<T>(0)) ? static_cast<T>(1) / sx : static_cast<T>(1);
        T invSy = (sy != static_cast<T>(0)) ? static_cast<T>(1) / sy : static_cast<T>(1);
        T invSz = (sz != static_cast<T>(0)) ? static_cast<T>(1) / sz : static_cast<T>(1);

        T r00 = m[0][0] * invSx;
        T r01 = m[0][1] * invSy;
        T r02 = m[0][2] * invSz;
        T r10 = m[1][0] * invSx;
        T r11 = m[1][1] * invSy;
        T r12 = m[1][2] * invSz;
        T r20 = m[2][0] * invSx;
        T r21 = m[2][1] * invSy;
        T r22 = m[2][2] * invSz;

        T trace = r00 + r11 + r22;
        T qw, qx, qy, qz;

        if (trace > static_cast<T>(0))
        {
            T s = std::sqrt(static_cast<double>(trace) + 1.0) * static_cast<T>(2);
            qw = static_cast<T>(0.25) * s;
            qx = (r21 - r12) / s;
            qy = (r02 - r20) / s;
            qz = (r10 - r01) / s;
        }
        else if ((r00 > r11) && (r00 > r22))
        {
            T s = std::sqrt(static_cast<double>(1.0 + r00 - r11 - r22)) * static_cast<T>(2);
            qw = (r21 - r12) / s;
            qx = static_cast<T>(0.25) * s;
            qy = (r01 + r10) / s;
            qz = (r02 + r20) / s;
        }
        else if (r11 > r22)
        {
            T s = std::sqrt(static_cast<double>(1.0 + r11 - r00 - r22)) * static_cast<T>(2);
            qw = (r02 - r20) / s;
            qx = (r01 + r10) / s;
            qy = static_cast<T>(0.25) * s;
            qz = (r12 + r21) / s;
        }
        else
        {
            T s = std::sqrt(static_cast<double>(1.0 + r22 - r00 - r11)) * static_cast<T>(2);
            qw = (r10 - r01) / s;
            qx = (r02 + r20) / s;
            qy = (r12 + r21) / s;
            qz = static_cast<T>(0.25) * s;
        }

        // Quaternion class uses floats; cast safely.
        return Quaternion(static_cast<float>(qw), static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz));
    }

    template <typename T>
    bool Mat4<T>::ValidTRS() const
    {
        // Check last row is (0,0,0,1) and upper-left 3x3 is orthonormal up to scale.
        const double EPS = 1e-5;
        if (std::fabs(static_cast<double>(m[0][3])) > EPS) return false;
        if (std::fabs(static_cast<double>(m[1][3])) > EPS) return false;
        if (std::fabs(static_cast<double>(m[2][3])) > EPS) return false;
        if (std::fabs(static_cast<double>(m[3][3] - static_cast<T>(1))) > EPS) return false;

        Vector3<T> col0 = GetColumn(0);
        Vector3<T> col1 = GetColumn(1);
        Vector3<T> col2 = GetColumn(2);

        // Non-zero scale?
        Vector3<T> scale = ExtractScale();
        if (scale.x == static_cast<T>(0) || scale.y == static_cast<T>(0) || scale.z == static_cast<T>(0))
            return false;

        // Normalize columns by scale and check orthonormality
        Vector3<T> n0 = Vector3<T>(col0.x / scale.x, col0.y / scale.x, col0.z / scale.x).Normalized();
        Vector3<T> n1 = Vector3<T>(col1.x / scale.y, col1.y / scale.y, col1.z / scale.y).Normalized();
        Vector3<T> n2 = Vector3<T>(col2.x / scale.z, col2.y / scale.z, col2.z / scale.z).Normalized();

        double d01 = std::fabs(static_cast<double>(n0.Dot(n1)));
        double d02 = std::fabs(static_cast<double>(n0.Dot(n2)));
        double d12 = std::fabs(static_cast<double>(n1.Dot(n2)));

        if (d01 > EPS || d02 > EPS || d12 > EPS) return false;

        return true;
    }

    template <typename T>
    bool Mat4<T>::operator==(const Mat4<T>& other) const
    {
        const double EPS = 1e-6;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (std::fabs(static_cast<double>(m[i][j] - other.m[i][j])) > EPS)
                    return false;
        return true;
    }

    template <typename T>
    bool Mat4<T>::operator!=(const Mat4<T>& other) const
    {
        return !(*this == other);
    }
} // gcep