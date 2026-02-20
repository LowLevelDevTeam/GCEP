#include "Vector2.hpp"

namespace gcep
{
    // Constructors
    template<typename T>
    constexpr Vector2<T>::Vector2() : x(0), y(0) {}

    template<typename T>
    constexpr Vector2<T>::Vector2(T x_, T y_) : x(x_), y(y_) {}

    template<typename T>
    template<typename U>
    constexpr Vector2<T>::Vector2(const Vector2<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {
    }

    // Arithmetic Operators
    template<typename T>
    constexpr Vector2<T> Vector2<T>::operator+(const Vector2& rhs) const { return { x + rhs.x, y + rhs.y }; }

    template<typename T>
    constexpr Vector2<T> Vector2<T>::operator-(const Vector2& rhs) const { return { x - rhs.x, y - rhs.y }; }

    template<typename T>
    constexpr Vector2<T>& Vector2<T>::operator+=(const Vector2& rhs) { x += rhs.x; y += rhs.y; return *this; }

    template<typename T>
    constexpr Vector2<T>& Vector2<T>::operator-=(const Vector2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }

    template<typename T>
    constexpr Vector2<T> Vector2<T>::operator*(T scalar) const { return { x * scalar, y * scalar }; }

    template<typename T>
    constexpr Vector2<T> Vector2<T>::operator/(T scalar) const { return { x / scalar, y / scalar }; }

    template<typename T>
    constexpr Vector2<T>& Vector2<T>::operator*=(T scalar) { x *= scalar; y *= scalar; return *this; }

    template<typename T>
    constexpr Vector2<T>& Vector2<T>::operator/=(T scalar) { x /= scalar; y /= scalar; return *this; }

    // Comparison
    template<typename T>
    constexpr bool Vector2<T>::Equals(const Vector2& rhs, T epsilon) const
    {
        return std::fabs(x - rhs.x) < epsilon && std::fabs(y - rhs.y) < epsilon;
    }

    template<typename T>
    constexpr bool Vector2<T>::operator==(const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }

    template<typename T>
    constexpr bool Vector2<T>::operator!=(const Vector2& rhs) const { return !(*this == rhs); }

    // Vector Math
    template<typename T>
    constexpr T Vector2<T>::Dot(const Vector2& rhs) const { return x * rhs.x + y * rhs.y; }

    template<typename T>
    T Vector2<T>::Length() const { return std::sqrt(x * x + y * y); }

    template<typename T>
    T Vector2<T>::LengthSquared() const { return x * x + y * y; }

    template<typename T>
    Vector2<T> Vector2<T>::Normalized() const
    {
        T len = Length();
        if (len == 0) return { 0, 0 };
        return { x / len, y / len };
    }

    template<typename T>
    void Vector2<T>::Normalize()
    {
        T len = Length();
        if (len != 0) { x /= len; y /= len; }
    }

    template<typename T>
    T Vector2<T>::Distance(const Vector2& a, const Vector2& b)
    {
        return (a - b).length();
    }

    template<typename T>
    Vector2<T> Vector2<T>::Lerp(const Vector2& a, const Vector2& b, T t)
    {
        return a + (b - a) * t;
    }

    template<typename T>
    T Vector2<T>::Angle(const Vector2& a, const Vector2& b)
    {
        T dot = a.Dot(b);
        T magnitudeProduct = a.Length() * b.Length();

        // Avoid division by zero
        if (magnitudeProduct == static_cast<T>(0))
            return static_cast<T>(0);

        T cosTheta = dot / magnitudeProduct;

        // Clamp the value to avoid domain errors with acos()
        if (cosTheta > static_cast<T>(1))
            cosTheta = static_cast<T>(1);
        else if (cosTheta < static_cast<T>(-1))
            cosTheta = static_cast<T>(-1);

        return std::acos(cosTheta) * static_cast<T>(180.0 / std::numbers::pi_v<float>);
    }

    template<typename T>
    Vector2<T> Vector2<T>::Perpendicular() const
    {
        // Returns a vector rotated 90 degrees counter-clockwise
        return Vector2<T>(-y, x);
    }

    template<typename T>
    Vector2<T> Vector2<T>::Scale(const Vector2& a, const Vector2& b)
    {
        return Vector2<T>(a.x * b.x, a.y * b.y);
    }

    template<typename T>
    Vector2<T> Vector2<T>::Min(const Vector2& a, const Vector2& b)
    {
        return Vector2<T>(
            (a.x < b.x) ? a.x : b.x,
            (a.y < b.y) ? a.y : b.y
        );
    }

    template<typename T>
    Vector2<T> Vector2<T>::Max(const Vector2& a, const Vector2& b)
    {
        return Vector2<T>(
            (a.x > b.x) ? a.x : b.x,
            (a.y > b.y) ? a.y : b.y
        );
    }

    template<typename T>
    Vector2<T> Vector2<T>::MoveTowards(const Vector2& current, const Vector2& target, T maxDistanceDelta)
    {
        Vector2<T> delta = target - current;
        T distance = delta.Length();

        if (distance <= maxDistanceDelta || distance == static_cast<T>(0))
            return target;

        return current + delta / distance * maxDistanceDelta;
    }
} // gcep