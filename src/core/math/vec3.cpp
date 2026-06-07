#include "core/math/vec3.hpp"

Vec3::Vec3()
    : x(0.0f), y(0.0f), z(0.0f) {}

Vec3::Vec3(f32 x, f32 y, f32 z)
    : x(x), y(y), z(z) {}

Vec3::Vec3(f32 n)
    : x(n), y(n), z(n) {}

Vec3 Vec3::operator+(const Vec3& other) const {
    return Vec3(
        x + other.x,
        y + other.y,
        z + other.z
    );
}

Vec3 Vec3::operator-(const Vec3& other) const {
    return Vec3(
        x - other.x,
        y - other.y,
        z - other.z
    );
}

Vec3 Vec3::operator*(f32 scalar) const {
    return Vec3(
        x * scalar,
        y * scalar,
        z * scalar
    );
}

Vec3 Vec3::operator/(f32 scalar) const {
    return Vec3(
        x / scalar,
        y / scalar,
        z / scalar
    );
}

Vec3 Vec3::operator-() const {
    return Vec3(
        -x,
        -y,
        -z
    );
}

Vec3& Vec3::operator+=(const Vec3& other) {
    x += other.x;
    y += other.y;
    z += other.z;

    return *this;
}

Vec3& Vec3::operator-=(const Vec3& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;

    return *this;
}

f32 Vec3::length() const {
    return std::sqrt(
        x * x +
        y * y +
        z * z
    );
}

Vec3 Vec3::normalized() const {
    f32 len = length();

    if (len == 0.0f) {
        return Vec3();
    }

    return Vec3(
        x / len,
        y / len,
        z / len
    );
}

f32 Vec3::dot(const Vec3& a, const Vec3& b) {
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

Vec3 Vec3::cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}