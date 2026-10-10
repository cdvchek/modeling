#pragma once

#include <cmath>
#include <types>

struct Vec2 {
    f32 x;
    f32 y;

    Vec2();
    Vec2(f32 x, f32 y);
    Vec2(f32 n);

    Vec2 operator+(const Vec2& other) const;
    Vec2 operator-(const Vec2& other) const;
    Vec2 operator*(f32 scalar) const;
    Vec2 operator/(f32 scalar) const;
    Vec2 operator-() const;
    
    Vec2& operator+=(const Vec2& other);
    Vec2& operator-=(const Vec2& other);

    float length() const;
    Vec2 normalized() const;

    static float dot(const Vec2& a, const Vec2& b);
};