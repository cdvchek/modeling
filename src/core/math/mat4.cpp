#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"
#include "core/math/vec4.hpp"
#include <cmath>

f32* Mat4::operator[](u32 column) {
    return &m[column * 4];
}

const f32* Mat4::operator[](u32 column) const {
    return &m[column * 4];
}

Mat4::Mat4() {
    for (u32 i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }
}

Mat4 Mat4::operator*(const Mat4& other) const {
    Mat4 result;

    for (u32 col = 0; col < 4; col++) {
        for (u32 row = 0; row < 4; row++) {
            result[col][row] =
                (*this)[0][row] * other[col][0] +
                (*this)[1][row] * other[col][1] +
                (*this)[2][row] * other[col][2] +
                (*this)[3][row] * other[col][3];
        }
    }

    return result;
}

Vec4 Mat4::operator*(const Vec4& v) const {
    return Vec4(
        (*this)[0][0] * v.x +
        (*this)[1][0] * v.y +
        (*this)[2][0] * v.z +
        (*this)[3][0] * v.w,

        (*this)[0][1] * v.x +
        (*this)[1][1] * v.y +
        (*this)[2][1] * v.z +
        (*this)[3][1] * v.w,

        (*this)[0][2] * v.x +
        (*this)[1][2] * v.y +
        (*this)[2][2] * v.z +
        (*this)[3][2] * v.w,

        (*this)[0][3] * v.x +
        (*this)[1][3] * v.y +
        (*this)[2][3] * v.z +
        (*this)[3][3] * v.w
    );
}

Mat4 Mat4::identity() {
    Mat4 result;

    result[0][0] = 1.0f;
    result[1][1] = 1.0f;
    result[2][2] = 1.0f;
    result[3][3] = 1.0f;

    return result;
}

Mat4 Mat4::translation(const Vec3& translation) {
    Mat4 result = identity();

    result[3][0] = translation.x;
    result[3][1] = translation.y;
    result[3][2] = translation.z;

    return result;
}

Mat4 Mat4::scale(const Vec3& scale) {
    Mat4 result = identity();

    result[0][0] = scale.x;
    result[1][1] = scale.y;
    result[2][2] = scale.z;

    return result;
}

Mat4 Mat4::rotationX(f32 radians) {
    Mat4 result = identity();

    f32 c = std::cos(radians);
    f32 s = std::sin(radians);

    result[1][1] =  c;
    result[1][2] =  s;

    result[2][1] = -s;
    result[2][2] =  c;

    return result;
}

Mat4 Mat4::rotationY(f32 radians) {
    Mat4 result = identity();

    f32 c = std::cos(radians);
    f32 s = std::sin(radians);

    result[0][0] =  c;
    result[0][2] = -s;

    result[2][0] =  s;
    result[2][2] =  c;

    return result;
}

Mat4 Mat4::rotationZ(f32 radians) {
    Mat4 result = identity();

    f32 c = std::cos(radians);
    f32 s = std::sin(radians);

    result[0][0] =  c;
    result[0][1] =  s;

    result[1][0] = -s;
    result[1][1] =  c;

    return result;
}

Mat4 Mat4::perspective(f32 fovRadians, f32 aspectRatio, f32 nearPlane, f32 farPlane) {
    Mat4 result;

    f32 tanHalfFov = std::tan(fovRadians * 0.5f);

    result[0][0] = 1.0f / (aspectRatio * tanHalfFov);
    result[1][1] = 1.0f / tanHalfFov;

    result[2][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result[2][3] = -1.0f;

    result[3][2] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);

    return result;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 forward = (target - eye).normalized();

    Vec3 right = Vec3::cross(forward, up).normalized();

    Vec3 cameraUp = Vec3::cross(right, forward);

    Mat4 result = identity();

    result[0][0] = right.x;
    result[1][0] = right.y;
    result[2][0] = right.z;

    result[0][1] = cameraUp.x;
    result[1][1] = cameraUp.y;
    result[2][1] = cameraUp.z;

    result[0][2] = -forward.x;
    result[1][2] = -forward.y;
    result[2][2] = -forward.z;

    result[3][0] = -Vec3::dot(right, eye);
    result[3][1] = -Vec3::dot(cameraUp, eye);
    result[3][2] =  Vec3::dot(forward, eye);

    return result;
}

Mat4 Mat4::inverse(const Mat4& matrix) {
    Mat4 result;

    const f32* m = matrix.m;
    f32* inv = result.m;

    inv[0] = m[5]  * m[10] * m[15] -
             m[5]  * m[11] * m[14] -
             m[9]  * m[6]  * m[15] +
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] -
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] +
              m[4]  * m[11] * m[14] +
              m[8]  * m[6]  * m[15] -
              m[8]  * m[7]  * m[14] -
              m[12] * m[6]  * m[11] +
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] -
             m[4]  * m[11] * m[13] -
             m[8]  * m[5] * m[15] +
             m[8]  * m[7] * m[13] +
             m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] +
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] -
               m[8]  * m[6] * m[13] -
               m[12] * m[5] * m[10] +
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] +
              m[1]  * m[11] * m[14] +
              m[9]  * m[2] * m[15] -
              m[9]  * m[3] * m[14] -
              m[13] * m[2] * m[11] +
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] -
             m[0]  * m[11] * m[14] -
             m[8]  * m[2] * m[15] +
             m[8]  * m[3] * m[14] +
             m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] +
              m[0]  * m[11] * m[13] +
              m[8]  * m[1] * m[15] -
              m[8]  * m[3] * m[13] -
              m[12] * m[1] * m[11] +
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] -
              m[0]  * m[10] * m[13] -
              m[8]  * m[1] * m[14] +
              m[8]  * m[2] * m[13] +
              m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] -
             m[1]  * m[7] * m[14] -
             m[5]  * m[2] * m[15] +
             m[5]  * m[3] * m[14] +
             m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] +
              m[0]  * m[7] * m[14] +
              m[4]  * m[2] * m[15] -
              m[4]  * m[3] * m[14] -
              m[12] * m[2] * m[7] +
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] -
              m[0]  * m[7] * m[13] -
              m[4]  * m[1] * m[15] +
              m[4]  * m[3] * m[13] +
              m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] +
               m[0]  * m[6] * m[13] +
               m[4]  * m[1] * m[14] -
               m[4]  * m[2] * m[13] -
               m[12] * m[1] * m[6] +
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
              m[1] * m[7] * m[10] +
              m[5] * m[2] * m[11] -
              m[5] * m[3] * m[10] -
              m[9] * m[2] * m[7] +
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
             m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
               m[0] * m[7] * m[9] +
               m[4] * m[1] * m[11] -
               m[4] * m[3] * m[9] -
               m[8] * m[1] * m[7] +
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
              m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    f32 determinant =
        m[0] * inv[0] +
        m[1] * inv[4] +
        m[2] * inv[8] +
        m[3] * inv[12];

    if (determinant == 0.0f) {
        return Mat4::identity();
    }

    determinant = 1.0f / determinant;

    for (u32 i = 0; i < 16; i++) {
        inv[i] *= determinant;
    }

    return result;
}