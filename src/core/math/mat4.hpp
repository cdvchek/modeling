#pragma once

#include <types>

struct Vec3;
struct Vec4;

struct Mat4 {
    f32 m[16];

    Mat4();

    f32* operator[](u32 column);
    const f32* operator[](u32 column) const;

    Mat4 operator*(const Mat4& other) const;
    Vec4 operator*(const Vec4& other) const;

    static Mat4 identity();

    static Mat4 translation(const Vec3& translation);
    static Mat4 scale(const Vec3& translation);

    static Mat4 rotationX(f32 radians);
    static Mat4 rotationY(f32 radians);
    static Mat4 rotationZ(f32 radians);

    static Mat4 perspective(
        f32 fovRadians,
        f32 aspectRatio,
        f32 nearPlane,
        f32 farPlane
    );

    static Mat4 lookAt(
        const Vec3& eye,
        const Vec3& target,
        const Vec3& up
    );

    static Mat4 inverse(const Mat4& matrix);
};