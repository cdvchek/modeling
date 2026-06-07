#include "core/math/vec4.hpp"

Vec4::Vec4()
    : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

Vec4::Vec4(f32 x, f32 y, f32 z, f32 w)
    : x(x), y(y), z(z), w(w) {}

Vec4::Vec4(f32 n)
    : x(n), y(n), z(n), w(n) {}

Vec4 Vec4::operator+(const Vec4& other) const {
    return Vec4(
        x + other.x,
        y + other.y,
        z + other.z,
        w + other.w
    );
}

Vec4 Vec4::operator-(const Vec4& other) const {
    return Vec4(
        x - other.x,
        y - other.y,
        z - other.z,
        w - other.w
    );
}

Vec4 Vec4::operator*(f32 scalar) const {
    return Vec4(
        x * scalar,
        y * scalar,
        z * scalar,
        w * scalar
    );
}

Vec4 Vec4::operator/(f32 scalar) const {
    return Vec4(
        x / scalar,
        y / scalar,
        z / scalar,
        w / scalar
    );
}

Vec4 Vec4::operator-() const {
    return Vec4(
        -x,
        -y,
        -z,
        -w
    );
}

Vec4& Vec4::operator+=(const Vec4& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    w += other.w;

    return *this;
}

Vec4& Vec4::operator-=(const Vec4& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    w -= other.w;

    return *this;
}

f32 Vec4::length() const {
    return std::sqrt(
        x * x +
        y * y +
        z * z +
        w * w
    );
}

Vec4 Vec4::normalized() const {
    f32 len = length();

    if (len == 0.0f) {
        return Vec4();
    }

    return Vec4(
        x / len,
        y / len,
        z / len,
        w / len
    );
}

f32 Vec4::dot(const Vec4& a, const Vec4& b) {
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z +
        a.w * b.w;
}