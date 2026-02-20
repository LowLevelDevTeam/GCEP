namespace gcep
{
    
    // Constructors
    template<typename T>
    constexpr Vector3<T>::Vector3() : x(0), y(0), z(0) {}

    template<typename T>
    constexpr Vector3<T>::Vector3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}

    template<typename T>
    template<typename U>
    constexpr Vector3<T>::Vector3(const Vector3<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {
    }

    // Arithmetic operators
    template<typename T>
    constexpr Vector3<T> Vector3<T>::operator+(const Vector3& rhs) const
    {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }

    template<typename T>
    constexpr Vector3<T> Vector3<T>::operator-(const Vector3& rhs) const
    {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }

    template<typename T>
    constexpr Vector3<T>& Vector3<T>::operator+=(const Vector3& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    template<typename T>
    constexpr Vector3<T>& Vector3<T>::operator-=(const Vector3& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    template<typename T>
    constexpr Vector3<T> Vector3<T>::operator*(T scalar) const
    {
        return { x * scalar, y * scalar, z * scalar };
    }

    template<typename T>
    constexpr Vector3<T> Vector3<T>::operator/(T scalar) const
    {
        return { x / scalar, y / scalar, z / scalar };
    }

    template<typename T>
    constexpr Vector3<T>& Vector3<T>::operator*=(T scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    template<typename T>
    constexpr Vector3<T>& Vector3<T>::operator/=(T scalar)
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    // Comparison
    template<typename T>
    constexpr bool Vector3<T>::Equals(const Vector3& rhs, T epsilon) const
    {
        return std::fabs(x - rhs.x) < epsilon &&
            std::fabs(y - rhs.y) < epsilon &&
            std::fabs(z - rhs.z) < epsilon;
    }

    template<typename T>
    constexpr bool Vector3<T>::operator==(const Vector3& rhs) const
    {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    template<typename T>
    constexpr bool Vector3<T>::operator!=(const Vector3& rhs) const
    {
        return !(*this == rhs);
    }

    // Dot and Cross products
    template<typename T>
    constexpr T Vector3<T>::Dot(const Vector3& rhs) const
    {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    template<typename T>
    constexpr Vector3<T> Vector3<T>::Cross(const Vector3& rhs) const
    {
        return Vector3<T>(
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        );
    }

    // Magnitude and normalization
    template<typename T>
    T Vector3<T>::Length() const
    {
        return std::sqrt(x * x + y * y + z * z);
    }

    template<typename T>
    T Vector3<T>::LengthSquared() const
    {
        return x * x + y * y + z * z;
    }

    template<typename T>
    Vector3<T> Vector3<T>::Normalized() const
    {
        T len = Length();
        return len > static_cast<T>(0) ? (*this / len) : Vector3<T>(0, 0, 0);
    }

    template<typename T>
    void Vector3<T>::Normalize()
    {
        T len = Length();
        if (len > static_cast<T>(0))
        {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    // Distance and interpolation
    template<typename T>
    T Vector3<T>::Distance(const Vector3& a, const Vector3& b)
    {
        return (a - b).Length();
    }

    template<typename T>
    Vector3<T> Vector3<T>::Lerp(const Vector3& a, const Vector3& b, T t)
    {
        return a + (b - a) * t;
    }

    // Utility
    template<typename T>
    Vector3<T> Vector3<T>::Perpendicular() const
    {
        // Returns a perpendicular vector (arbitrary, useful for generating orthogonal basis)
        return std::fabs(x) > std::fabs(z) ? Vector3<T>(-y, x, 0) : Vector3<T>(0, -z, y);
    }

    template<typename T>
    Vector3<T> Vector3<T>::Scale(const Vector3& a, const Vector3& b)
    {
        return { a.x * b.x, a.y * b.y, a.z * b.z };
    }

    template<typename T>
    Vector3<T> Vector3<T>::Min(const Vector3& a, const Vector3& b)
    {
        return {
            (a.x < b.x) ? a.x : b.x,
            (a.y < b.y) ? a.y : b.y,
            (a.z < b.z) ? a.z : b.z
        };
    }

    template<typename T>
    Vector3<T> Vector3<T>::Max(const Vector3& a, const Vector3& b)
    {
        return {
            (a.x > b.x) ? a.x : b.x,
            (a.y > b.y) ? a.y : b.y,
            (a.z > b.z) ? a.z : b.z
        };
    }

    template<typename T>
    Vector3<T> Vector3<T>::MoveTowards(const Vector3& current, const Vector3& target, T maxDistanceDelta)
    {
        Vector3<T> delta = target - current;
        T distance = delta.Length();
        if (distance <= maxDistanceDelta || distance == static_cast<T>(0))
            return target;

        return current + delta / distance * maxDistanceDelta;
    }

    template<typename T>
    Vector3<T> Vector3<T>::Reflect(const Vector3& direction, const Vector3& normal)
    {
        return direction - normal * (static_cast<T>(2) * direction.Dot(normal));
    }

    template<typename T>
    T Vector3<T>::Angle(const Vector3& a, const Vector3& b)
    {
        T dot = a.Dot(b);
        T mag = a.Length() * b.Length();
        if (mag == static_cast<T>(0))
            return static_cast<T>(0);

        T cosTheta = dot / mag;
        cosTheta = std::max(static_cast<T>(-1), std::min(static_cast<T>(1), cosTheta));
        return std::acos(cosTheta) * static_cast<T>(180.0 / std::numbers::pi_v<float>);
    }
} // gcep