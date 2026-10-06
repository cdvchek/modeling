#include "scene/transform.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
#include <cmath>

Transform::Transform() : position(0.0f), rotation(0.0f), scale(1.0f) {}

Mat4 Transform::getMatrix() const {
    Mat4 translation = Mat4::translation(position);
    Mat4 rotationX = Mat4::rotationX(rotation.x);
    Mat4 rotationY = Mat4::rotationY(rotation.y);
    Mat4 rotationZ = Mat4::rotationZ(rotation.z);
    Mat4 scaling = Mat4::scale(scale);

    return translation * rotationZ * rotationY * rotationX * scaling;
}

ObjectSpace::ObjectSpace(const Transform& transform) : toWorld(transform.getMatrix()), toLocal(Mat4::inverse(toWorld)) {}

Vec3 ObjectSpace::pointToWorld(const Vec3& point) const {
    const Vec4 result = toWorld * Vec4(point.x, point.y, point.z, 1.0f);
    return Vec3(result.x, result.y, result.z);
}

Vec3 ObjectSpace::pointToLocal(const Vec3& point) const {
    const Vec4 result = toLocal * Vec4(point.x, point.y, point.z, 1.0f);
    return Vec3(result.x, result.y, result.z);
}

Vec3 ObjectSpace::directionToLocal(const Vec3& direction) const {
    const Vec4 result = toLocal * Vec4(direction.x, direction.y, direction.z, 0.0f);
    return Vec3(result.x, result.y, result.z);
}

namespace {
    // Row-major 3x3, rows then columns, in the usual math notation
    struct Mat3 {
        f32 m[3][3];
    };

    Mat3 multiply(const Mat3& a, const Mat3& b) {
        Mat3 result {};
        for (u32 row = 0; row < 3; ++row) {
            for (u32 col = 0; col < 3; ++col) {
                for (u32 k = 0; k < 3; ++k) result.m[row][col] += a.m[row][k] * b.m[k][col];
            }
        }
        return result;
    }

    Mat3 eulerToMatrix(const Vec3& euler) {
        const f32 cx = std::cos(euler.x), sx = std::sin(euler.x);
        const f32 cy = std::cos(euler.y), sy = std::sin(euler.y);
        const f32 cz = std::cos(euler.z), sz = std::sin(euler.z);

        const Mat3 rx { { { 1, 0, 0 }, { 0, cx, -sx }, { 0, sx, cx } } };
        const Mat3 ry { { { cy, 0, sy }, { 0, 1, 0 }, { -sy, 0, cy } } };
        const Mat3 rz { { { cz, -sz, 0 }, { sz, cz, 0 }, { 0, 0, 1 } } };
        return multiply(rz, multiply(ry, rx));
    }

    Mat3 axisAngleToMatrix(const Vec3& axis, f32 angle) {
        const Vec3 a = axis.normalized();
        const f32 c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;

        return { {
            { t * a.x * a.x + c,       t * a.x * a.y - s * a.z, t * a.x * a.z + s * a.y },
            { t * a.x * a.y + s * a.z, t * a.y * a.y + c,       t * a.y * a.z - s * a.x },
            { t * a.x * a.z - s * a.y, t * a.y * a.z + s * a.x, t * a.z * a.z + c }
        } };
    }

    Vec3 matrixToEuler(const Mat3& r) {
        const f32 sy = std::clamp(-r.m[2][0], -1.0f, 1.0f);
        const f32 y = std::asin(sy);

        // Looking straight along Y, X and Z turn about the same axis; put it all in Z
        if (std::abs(sy) > 0.9999f) {
            return Vec3(0.0f, y, std::atan2(-r.m[0][1], r.m[1][1]));
        }

        return Vec3(std::atan2(r.m[2][1], r.m[2][2]), y, std::atan2(r.m[1][0], r.m[0][0]));
    }
}

Vec3 rotateEuler(const Vec3& euler, const Vec3& axis, f32 angle) {
    return matrixToEuler(multiply(axisAngleToMatrix(axis, angle), eulerToMatrix(euler)));
}
