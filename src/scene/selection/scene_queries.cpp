#include "scene/selection/scene_queries.hpp"

#include <cfloat>
#include <algorithm>

#include "scene/scene.hpp"
#include "scene/object_collection.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "core/math/vec4.hpp"
#include "core/math/math_utils.hpp"

static bool rayHitsPoint(
    const Ray& ray,
    const Vec3& point,
    f32 radius,
    f32& distanceOut
) {
    Vec3 toPoint = point - ray.origin;

    f32 t = Vec3::dot(toPoint, ray.direction);

    if (t < 0.0f) {
        return false;
    }

    Vec3 closestPoint = ray.origin + ray.direction * t;
    Vec3 difference = point - closestPoint;

    f32 distanceSq = Vec3::dot(difference, difference);
    f32 radiusSq = radius * radius;

    if (distanceSq > radiusSq) {
        return false;
    }

    distanceOut = t;
    return true;
}

VertexHit pickVertex(const Scene& scene, const Ray& ray, f32 radius) {
    VertexHit bestHit;

    for (u32 objectIndex = 0; objectIndex < scene.objects.count(); ++objectIndex) {
        const Object& object = scene.objects.get(objectIndex);
        const Mat4 model = object.transform.getMatrix();

        for (const VertexHandle& vertexHandle : object.meshData.getVertexHandles()) {
            const Vec3 vertexPos = object.meshData.getVertexPosition(vertexHandle);

            const Vec4 worldPos4 = model * Vec4(
                vertexPos.x,
                vertexPos.y,
                vertexPos.z,
                1.0f
            );

            const Vec3 worldPos(
                worldPos4.x,
                worldPos4.y,
                worldPos4.z
            );

            f32 distance = 0.0f;

            if (!rayHitsPoint(
                    ray,
                    worldPos,
                    radius,
                    distance)) {
                continue;
            }

            if (distance < bestHit.distance) {
                bestHit.hit = true;
                bestHit.objectIndex = objectIndex;
                bestHit.vertex = vertexHandle;
                bestHit.distance = distance;
            }
        }
    }

    return bestHit;
}

bool rayHitsEdge(
    const Ray& ray,
    const Vec3& a,
    const Vec3& b,
    f32 radius,
    f32& distance
) {
    const Vec3 edgeDir = b - a;
    const Vec3 rayToA = ray.origin - a;

    const f32 edgeLengthSq = Vec3::dot(edgeDir, edgeDir);

    if (edgeLengthSq <= 0.000001f) {
        return false;
    }

    const f32 aDot = Vec3::dot(ray.direction, ray.direction);
    const f32 bDot = Vec3::dot(ray.direction, edgeDir);
    const f32 cDot = edgeLengthSq;
    const f32 dDot = Vec3::dot(ray.direction, rayToA);
    const f32 eDot = Vec3::dot(edgeDir, rayToA);

    const f32 denominator = aDot * cDot - bDot * bDot;

    f32 rayT;
    f32 edgeT;

    if (std::abs(denominator) > 0.000001f) {
        rayT = (bDot * eDot - cDot * dDot) / denominator;
        edgeT = (aDot * eDot - bDot * dDot) / denominator;
    } else {
        // Ray and edge are nearly parallel.
        rayT = 0.0f;
        edgeT = eDot / cDot;
    }

    // The edge is a segment, so clamp to [0, 1].
    edgeT = std::clamp(edgeT, 0.0f, 1.0f);

    // Picking should only happen in front of the camera.
    if (rayT < 0.0f) {
        rayT = 0.0f;
    }

    // Once edgeT has been clamped, recompute the nearest
    // point on the ray to that selected point on the edge.
    const Vec3 edgePoint = a + edgeDir * edgeT;

    rayT = Vec3::dot(edgePoint - ray.origin, ray.direction)
         / Vec3::dot(ray.direction, ray.direction);

    if (rayT < 0.0f) {
        return false;
    }

    const Vec3 rayPoint = ray.origin + ray.direction * rayT;

    const f32 separation = (edgePoint - rayPoint).length();

    if (separation > radius) {
        return false;
    }

    distance = rayT;

    return true;
}

EdgeHit pickEdge(
    const Scene& scene,
    const Ray& ray,
    f32 radius
) {
    EdgeHit bestHit;

    for (u32 objectIndex = 0;
         objectIndex < scene.objects.count();
         ++objectIndex) {

        const Object& object = scene.objects.get(objectIndex);

        const Mat4 model = object.transform.getMatrix();

        for (const EdgeHandle& edgeHandle : object.meshData.getEdgeHandles()) {
            const VertexHandle originHandle = object.meshData.getEdgeOrigin(edgeHandle);

            const VertexHandle tipHandle = object.meshData.getEdgeTip(edgeHandle);

            if (!object.meshData.isValidHandle(originHandle) ||
                !object.meshData.isValidHandle(tipHandle)) {
                continue;
            }

            const Vec3 originPos = object.meshData.getVertexPosition(originHandle);

            const Vec3 tipPos = object.meshData.getVertexPosition(tipHandle);

            const Vec4 worldOrigin4 = model * Vec4(
                originPos.x,
                originPos.y,
                originPos.z,
                1.0f
            );

            const Vec4 worldTip4 = model * Vec4(
                tipPos.x,
                tipPos.y,
                tipPos.z,
                1.0f
            );

            const Vec3 worldOrigin(
                worldOrigin4.x,
                worldOrigin4.y,
                worldOrigin4.z
            );

            const Vec3 worldTip(
                worldTip4.x,
                worldTip4.y,
                worldTip4.z
            );

            f32 distance = 0.0f;

            if (!rayHitsEdge(
                    ray,
                    worldOrigin,
                    worldTip,
                    radius,
                    distance)) {
                continue;
            }

            if (distance < bestHit.distance) {
                bestHit.hit = true;
                bestHit.objectIndex = objectIndex;
                bestHit.edge = edgeHandle;
                bestHit.distance = distance;
            }
        }
    }

    return bestHit;
}

static bool rayHitsTriangle(
    const Ray& ray,
    const Vec3& p1,
    const Vec3& p2,
    const Vec3& p3,
    f32& distanceOut
) {
    const Vec3 edge1 = p2 - p1;
    const Vec3 edge2 = p3 - p1;

    const Vec3 h = Vec3::cross(ray.direction, edge2);
    const f32 determinant = Vec3::dot(edge1, h);

    // Ray is parallel to the triangle.
    if (std::abs(determinant) < Math::EPSILON) {
        return false;
    }

    const f32 invDet = 1.0f / determinant;

    const Vec3 s = ray.origin - p1;
    const f32 u = invDet * Vec3::dot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const Vec3 q = Vec3::cross(s, edge1);
    const f32 v = invDet * Vec3::dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    const f32 t = invDet * Vec3::dot(edge2, q);

    if (t < 0.0f) {
        return false;
    }

    distanceOut = t;
    return true;
}

FaceHit pickFace(const Scene& scene, const Ray& ray) {
    FaceHit bestHit;
    bestHit.distance = FLT_MAX;

    for (u32 objectIndex = 0; objectIndex < scene.objects.count(); ++objectIndex) {
        const Object& object = scene.objects.get(objectIndex);
        Mat4 model = object.transform.getMatrix();

        for (const FaceHandle faceHandle : object.meshData.getFaceHandles()) {
            const auto& triangles = object.meshData.getFaceTriangles(faceHandle);

            for (const Triangle& triangle : triangles) {
                const Vec3 v1Pos = object.meshData.getVertexPosition(triangle.v0);
                const Vec3 v2Pos = object.meshData.getVertexPosition(triangle.v1);
                const Vec3 v3Pos = object.meshData.getVertexPosition(triangle.v2);
                
                const Vec4 worldPos4P1 = model * Vec4(v1Pos.x, v1Pos.y, v1Pos.z, 1.0f);
                const Vec4 worldPos4P2 = model * Vec4(v2Pos.x, v2Pos.y, v2Pos.z, 1.0f);
                const Vec4 worldPos4P3 = model * Vec4(v3Pos.x, v3Pos.y, v3Pos.z, 1.0f);

                const Vec3 worldPosP1(
                    worldPos4P1.x,
                    worldPos4P1.y,
                    worldPos4P1.z
                );

                const Vec3 worldPosP2(
                    worldPos4P2.x,
                    worldPos4P2.y,
                    worldPos4P2.z
                );

                const Vec3 worldPosP3(
                    worldPos4P3.x,
                    worldPos4P3.y,
                    worldPos4P3.z
                );

                f32 distance = 0.0f;

                if (!rayHitsTriangle(ray, worldPosP1, worldPosP2, worldPosP3, distance)) continue;

                if (distance < bestHit.distance) {
                    bestHit.hit = true;
                    bestHit.objectIndex = objectIndex;
                    bestHit.face = faceHandle;
                    bestHit.distance = distance;
                }
            }
        }
    }

    return bestHit;
}