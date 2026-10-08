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
        // atan2 rather than asin, which loses precision near straight up or down
        const f32 y = std::atan2(-r.m[2][0], std::sqrt(r.m[0][0] * r.m[0][0] + r.m[1][0] * r.m[1][0]));

        // Looking straight along Y, X and Z turn about the same axis; put it all in Z
        // Adding 0 turns -0 into 0, so angles don't read back as "-0"
        if (std::abs(r.m[2][0]) > 0.99999f) {
            return Vec3(0.0f, y + 0.0f, std::atan2(-r.m[0][1], r.m[1][1]) + 0.0f);
        }

        return Vec3(std::atan2(r.m[2][1], r.m[2][2]) + 0.0f, y + 0.0f, std::atan2(r.m[1][0], r.m[0][0]) + 0.0f);
    }

    Mat3 transpose(const Mat3& a) {
        Mat3 result {};
        for (u32 row = 0; row < 3; ++row) for (u32 col = 0; col < 3; ++col) result.m[row][col] = a.m[col][row];
        return result;
    }

    Vec3 apply(const Mat3& a, const Vec3& v) {
        return Vec3(a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
                    a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
                    a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z);
    }

    Vec3 multiplyEach(const Vec3& a, const Vec3& b) {
        return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
    }

    Vec3 divideEach(const Vec3& a, const Vec3& b) {
        return Vec3(a.x / b.x, a.y / b.y, a.z / b.z);
    }
}

Vec3 eulerFromAxes(const Vec3& x, const Vec3& y, const Vec3& z) {
    // The axes are the rotation matrix's columns
    return matrixToEuler({ { { x.x, y.x, z.x }, { x.y, y.y, z.y }, { x.z, y.z, z.z } } });
}

Vec3 rotateEuler(const Vec3& euler, const Vec3& axis, f32 angle) {
    return matrixToEuler(multiply(axisAngleToMatrix(axis, angle), eulerToMatrix(euler)));
}

Transform combineTransforms(const Transform& parent, const Transform& local) {
    const Mat3 parentRotation = eulerToMatrix(parent.rotation);

    Transform world;
    world.position = parent.position + apply(parentRotation, multiplyEach(parent.scale, local.position));
    world.rotation = matrixToEuler(multiply(parentRotation, eulerToMatrix(local.rotation)));
    world.scale = multiplyEach(parent.scale, local.scale);
    return world;
}

Transform relativeTransform(const Transform& parent, const Transform& world) {
    const Mat3 inverseRotation = transpose(eulerToMatrix(parent.rotation));

    Transform local;
    local.position = divideEach(apply(inverseRotation, world.position - parent.position), parent.scale);
    local.rotation = matrixToEuler(multiply(inverseRotation, eulerToMatrix(world.rotation)));
    local.scale = divideEach(world.scale, parent.scale);
    return local;
}
