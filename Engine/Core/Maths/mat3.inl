namespace gcep
{
    // Constructors
    template<typename T>
    constexpr Mat3<T>::Mat3()
    {
        *this = Identity();
    }

    template<typename T>
    constexpr Mat3<T>::Mat3(
        T m00, T m01, T m02,
        T m10, T m11, T m12,
        T m20, T m21, T m22
    )
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    // Static methods
    template<typename T>
    Mat3<T> Mat3<T>::Identity()
    {
        return Mat3<T>(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        );
    }

    template<typename T>
    Mat3<T> Mat3<T>::RotationX(T angle)
    {
        T c = std::cos(angle);
        T s = std::sin(angle);
        return Mat3<T>(
            1, 0, 0,
            0, c, -s,
            0, s, c
        );
    }

    template<typename T>
    Mat3<T> Mat3<T>::RotationY(T angle)
    {
        T c = std::cos(angle);
        T s = std::sin(angle);
        return Mat3<T>(
            c, 0, s,
            0, 1, 0,
            -s, 0, c
        );
    }

    template<typename T>
    Mat3<T> Mat3<T>::RotationZ(T angle)
    {
        T c = std::cos(angle);
        T s = std::sin(angle);
        return Mat3<T>(
            c, -s, 0,
            s, c, 0,
            0, 0, 1
        );
    }

    template<typename T>
    Mat3<T> Mat3<T>::Scale(T x, T y, T z)
    {
        return Mat3<T>(
            x, 0, 0,
            0, y, 0,
            0, 0, z
        );
    }

    // Math
    template<typename T>
    Mat3<T> Mat3<T>::Transposed() const
    {
        return Mat3<T>(
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2]
        );
    }

    template<typename T>
    T Mat3<T>::Determinant() const
    {
        return
            m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
            m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
            m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    template<typename T>
    Mat3<T> Mat3<T>::Inverted() const
    {
        T det = Determinant();

        // If the determinant is zero, the matrix is not invertible.
        // Return the identity matrix instead.
        if (std::abs(det) < static_cast<T>(1e-6))
            return Mat3<T>::Identity();

        T invDet = static_cast<T>(1) / det;

        return Mat3<T>(
            (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet,
            (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet,
            (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet,

            (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet,
            (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet,
            (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet,

            (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet,
            (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet,
            (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet
        );
    }

    // Operators
    template<typename T>
    Mat3<T> Mat3<T>::operator*(const Mat3& rhs) const
    {
        Mat3<T> result;
        for (int r = 0; r < 3; ++r)
        {
            for (int c = 0; c < 3; ++c)
            {
                result.m[r][c] =
                    m[r][0] * rhs.m[0][c] +
                    m[r][1] * rhs.m[1][c] +
                    m[r][2] * rhs.m[2][c];
            }
        }
        return result;
    }

    template<typename T>
    Vector3<T> Mat3<T>::operator*(const Vector3<T>& v) const
    {
        return Vector3<T>(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }

    // Access
    template<typename T>
    T& Mat3<T>::operator()(int row, int col)
    {
        return m[row][col];
    }

    template<typename T>
    const T& Mat3<T>::operator()(int row, int col) const
    {
        return m[row][col];
    }

    template<typename T>
    Vector3<T> Mat3<T>::GetRow(int i) const
    {
        return Vector3<T>(m[i][0], m[i][1], m[i][2]);
    }

    template<typename T>
    void Mat3<T>::SetRow(int i, const Vector3<T>& row)
    {
        m[i][0] = row.x;
        m[i][1] = row.y;
        m[i][2] = row.z;
    }

    template<typename T>
    Vector3<T> Mat3<T>::GetColumn(int i) const
    {
        return Vector3<T>(m[0][i], m[1][i], m[2][i]);
    }

    template<typename T>
    void Mat3<T>::SetColumn(int i, const Vector3<T>& column)
    {
        m[0][i] = column.x;
        m[1][i] = column.y;
        m[2][i] = column.z;
    }

    // Helper
    template<typename T>
    T Mat3<T>::ToRadians(T degrees)
    {
        return degrees * static_cast<T>(std::numbers::pi_v<float> / 180.0);
    }
} // gcep