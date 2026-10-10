#pragma once

#include <cmath>
#include <types>

struct Vec4 {
    f32 x;
    f32 y;
    f32 z;
    f32 w;

    Vec4();
    Vec4(f32 x, f32 y, f32 z, f32 w);
    Vec4(f32 n);

    Vec4 operator+(const Vec4& other) const;
    Vec4 operator-(const Vec4& other) const;
    Vec4 operator*(f32 scalar) const;
    Vec4 operator/(f32 scalar) const;
    Vec4 operator-() const;

    Vec4& operator+=(const Vec4& other);
    Vec4& operator-=(const Vec4& other);

    f32 length() const;
    Vec4 normalized() const;

    static f32 dot(const Vec4& a, const Vec4& b);
};