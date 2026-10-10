#include "core/math/vec2.hpp"

Vec2::Vec2() : x(0.0f), y(0.0f) {};
Vec2::Vec2(f32 x, f32 y) : x(x), y(y) {};
Vec2::Vec2(f32 n) : x(n), y(n) {};

Vec2 Vec2::operator+(const Vec2& other) const {
    return Vec2(
        x + other.x,
        y + other.y
    );
}

Vec2 Vec2::operator-(const Vec2& other) const {
    return Vec2(
        x - other.x,
        y - other.y
    );
}

Vec2 Vec2::operator*(f32 scalar) const {
    return Vec2(
        x * scalar,
        y * scalar
    );
}

Vec2 Vec2::operator/(f32 scalar) const {
    return Vec2(
        x / scalar,
        y / scalar
    );
}

Vec2 Vec2::operator-() const {
    return Vec2(
        -x,
        -y
    );
}

Vec2& Vec2::operator+=(const Vec2& other) {
    x += other.x;
    y += other.y;
    return *this;
}

Vec2& Vec2::operator-=(const Vec2& other) {
    x -= other.x;
    y -= other.y;
    return *this;
}

float Vec2::length() const {
    return std::sqrt(x * x + y * y);
}

Vec2 Vec2::normalized() const {
    float len = length();

    if (len == 0.0f) {
        return Vec2();
    }

    return Vec2(
        x / len,
        y / len
    );
}

float Vec2::dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}