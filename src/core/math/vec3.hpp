#pragma once

#include <cmath>
#include <types>

struct Vec3 {
    f32 x;
    f32 y;
    f32 z;

    Vec3();
    Vec3(f32 x, f32 y, f32 z);
    Vec3(f32 n);

    Vec3 operator+(const Vec3& other) const;
    Vec3 operator-(const Vec3& other) const;
    Vec3 operator*(f32 scalar) const;
    Vec3 operator/(f32 scalar) const;
    Vec3 operator-() const;

    Vec3& operator+=(const Vec3& other);
    Vec3& operator-=(const Vec3& other);

    f32 length() const;
    Vec3 normalized() const;

    static f32 dot(const Vec3& a, const Vec3& b);
    static Vec3 cross(const Vec3& a, const Vec3& b);
};