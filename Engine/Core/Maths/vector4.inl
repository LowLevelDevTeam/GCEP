namespace gcep
{
    // Constructors
    template<typename T>
    constexpr Vector4<T>::Vector4() : x(0), y(0), z(0), w(0) {}

    template<typename T>
    constexpr Vector4<T>::Vector4(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}

    template<typename T>
    template<typename U>
    constexpr Vector4<T>::Vector4(const Vector4<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)),
          z(static_cast<T>(other.z)), w(static_cast<T>(other.w)) {}

    // Arithmetic operators
    template<typename T>
    constexpr Vector4<T> Vector4<T>::operator+(const Vector4& rhs) const
    {
        return { x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w };
    }

    template<typename T>
    constexpr Vector4<T> Vector4<T>::operator-(const Vector4& rhs) const
    {
        return { x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w };
    }

    template<typename T>
    constexpr Vector4<T>& Vector4<T>::operator+=(const Vector4& rhs)
    {
        x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w;
        return *this;
    }

    template<typename T>
    constexpr Vector4<T>& Vector4<T>::operator-=(const Vector4& rhs)
    {
        x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w;
        return *this;
    }

    template<typename T>
    constexpr Vector4<T> Vector4<T>::operator*(T scalar) const
    {
        return { x * scalar, y * scalar, z * scalar, w * scalar };
    }

    template<typename T>
    constexpr Vector4<T> Vector4<T>::operator/(T scalar) const
    {
        return { x / scalar, y / scalar, z / scalar, w / scalar };
    }

    template<typename T>
    constexpr Vector4<T>& Vector4<T>::operator*=(T scalar)
    {
        x *= scalar; y *= scalar; z *= scalar; w *= scalar;
        return *this;
    }

    template<typename T>
    constexpr Vector4<T>& Vector4<T>::operator/=(T scalar)
    {
        x /= scalar; y /= scalar; z /= scalar; w /= scalar;
        return *this;
    }

    // Comparison
    template<typename T>
    constexpr bool Vector4<T>::Equals(const Vector4& rhs, T epsilon) const
    {
        return std::fabs(x - rhs.x) < epsilon &&
               std::fabs(y - rhs.y) < epsilon &&
               std::fabs(z - rhs.z) < epsilon &&
               std::fabs(w - rhs.w) < epsilon;
    }

    template<typename T>
    constexpr bool Vector4<T>::operator==(const Vector4& rhs) const
    {
        return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
    }

    template<typename T>
    constexpr bool Vector4<T>::operator!=(const Vector4& rhs) const
    {
        return !(*this == rhs);
    }

    // Dot product
    template<typename T>
    constexpr T Vector4<T>::Dot(const Vector4& rhs) const
    {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }

    // Magnitude and normalization
    template<typename T>
    T Vector4<T>::Length() const
    {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    template<typename T>
    T Vector4<T>::LengthSquared() const
    {
        return x * x + y * y + z * z + w * w;
    }

    template<typename T>
    Vector4<T> Vector4<T>::Normalized() const
    {
        T len = Length();
        return len > static_cast<T>(0) ? (*this / len) : Vector4<T>(0, 0, 0, 0);
    }

    template<typename T>
    void Vector4<T>::Normalize()
    {
        T len = Length();
        if (len > static_cast<T>(0))
        {
            x /= len; y /= len; z /= len; w /= len;
        }
    }

    // Distance and interpolation
    template<typename T>
    T Vector4<T>::Distance(const Vector4& a, const Vector4& b)
    {
        return (a - b).Length();
    }

    template<typename T>
    Vector4<T> Vector4<T>::Lerp(const Vector4& a, const Vector4& b, T t)
    {
        return a + (b - a) * t;
    }

    // Utility
    template<typename T>
    Vector4<T> Vector4<T>::Scale(const Vector4& a, const Vector4& b)
    {
        return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w };
    }

    template<typename T>
    Vector4<T> Vector4<T>::Min(const Vector4& a, const Vector4& b)
    {
        return {
            (a.x < b.x) ? a.x : b.x,
            (a.y < b.y) ? a.y : b.y,
            (a.z < b.z) ? a.z : b.z,
            (a.w < b.w) ? a.w : b.w
        };
    }

    template<typename T>
    Vector4<T> Vector4<T>::Max(const Vector4& a, const Vector4& b)
    {
        return {
            (a.x > b.x) ? a.x : b.x,
            (a.y > b.y) ? a.y : b.y,
            (a.z > b.z) ? a.z : b.z,
            (a.w > b.w) ? a.w : b.w
        };
    }

    template<typename T>
    Vector4<T> Vector4<T>::MoveTowards(const Vector4& current, const Vector4& target, T maxDistanceDelta)
    {
        Vector4<T> delta = target - current;
        T distance = delta.Length();
        if (distance <= maxDistanceDelta || distance == static_cast<T>(0))
            return target;

        return current + delta / distance * maxDistanceDelta;
    }

    template<typename T>
    Vector4<T> Vector4<T>::Reflect(const Vector4& direction, const Vector4& normal)
    {
        return direction - normal * (static_cast<T>(2) * direction.Dot(normal));
    }

    template<typename T>
    T Vector4<T>::Angle(const Vector4& a, const Vector4& b)
    {
        T mag = a.Length() * b.Length();
        if (mag == static_cast<T>(0))
            return static_cast<T>(0);

        T cosTheta = a.Dot(b) / mag;
        cosTheta = std::max(static_cast<T>(-1), std::min(static_cast<T>(1), cosTheta));
        return std::acos(cosTheta) * static_cast<T>(180.0 / std::numbers::pi_v<double>);
    }
} // namespace gcep
