#ifndef NEKOLIB_VEC2_H
#define NEKOLIB_VEC2_H

#include <type_traits>
#include <cmath>

namespace neko {
template<typename T>
class Vec2
{
public:
    T x{}, y{};
    Vec2<T>()= default;
    Vec2<T>(T x, T y): x(x), y(y){}

    template<class U>
    constexpr explicit Vec2<T>(Vec2<U> v): x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

    [[nodiscard]] constexpr Vec2<T> operator+(Vec2<T> other) const
    {
        return {x + other.x, y + other.y};
    }

    constexpr Vec2<T>& operator+=(Vec2<T> other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    [[nodiscard]] constexpr Vec2<T> operator-(Vec2<T> other) const
    {
        return {x - other.x, y - other.y};
    }

    [[nodiscard]] constexpr Vec2<T> operator-() const
    {
        return {-x, -y};
    }

    constexpr Vec2<T>& operator-=(Vec2<T> other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    [[nodiscard]] constexpr Vec2<T> operator*(T other) const
    {
        return {x * other, y * other};
    }

    [[nodiscard]] constexpr Vec2<T> operator*(Vec2<T> other) const
    {
        return {x * other.x, y * other.y};
    }

    constexpr Vec2<T>& operator*=(T other)
    {
        x *= other;
        y *= other;
        return *this;
    }

    [[nodiscard]] constexpr Vec2<T> operator/(T other) const
    {
        return {x / other, y / other};
    }

    constexpr Vec2<T>& operator/=(T other)
    {
        x /= other;
        y /= other;
        return *this;
    }

    [[nodiscard]] constexpr static T Dot(Vec2<T> v1, Vec2<T> v2)
    {
        return v1.x * v2.x + v1.y * v2.y;
    }

    [[nodiscard]] constexpr T SqrLength() const
    {
        return Dot(*this, *this);
    }

    [[nodiscard]] T Length() const
    {
        return std::sqrt(Dot(*this, *this));
    }

    [[nodiscard]] constexpr static T Lerp(Vec2<T> v1, Vec2<T> v2, T t)
    {
        return v1 + (v2 - v1) * t;
    }
};

template<typename T>
Vec2<T> operator*(T value, Vec2<T> vector)
{
    return {value*vector.x, value*vector.y};
}

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int>;
using Vec2u = Vec2<unsigned>;
}
#endif //NEKOLIB_VEC2_H
