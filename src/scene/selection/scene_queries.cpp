#include "scene/selection/scene_queries.hpp"

#include <cfloat>

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

VertexHit pickVertex(
    const Scene& scene,
    const Ray& ray,
    f32 radius
) {
    VertexHit bestHit;
    bestHit.distance = FLT_MAX;

    for (u32 objectIndex = 0; objectIndex < scene.objects.count(); objectIndex++) {
        const Object& object = scene.objects.get(objectIndex);

        Mat4 model = object.transform.getMatrix();

        for (u32 vertexIndex = 0; vertexIndex < (u32)object.meshData.getVertices().size(); vertexIndex++) {
            const Vertex& vertex = object.meshData.getVertices()[vertexIndex];

            Vec4 worldPos4 = model * Vec4(
                vertex.position.x,
                vertex.position.y,
                vertex.position.z,
                1.0f
            );

            Vec3 worldPos(
                worldPos4.x,
                worldPos4.y,
                worldPos4.z
            );

            f32 distance = 0.0f;

            if (!rayHitsPoint(ray, worldPos, radius, distance)) {
                continue;
            }

            if (distance < bestHit.distance) {
                bestHit.hit = true;
                bestHit.objectIndex = objectIndex;
                bestHit.vertexIndex = vertexIndex;
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

FaceHit pickFace(const Scene& scene, const Ray& ray, f32 radius) {
    FaceHit bestHit;
    bestHit.distance = FLT_MAX;

    for (u32 objectIndex = 0; objectIndex < scene.objects.count(); objectIndex++) {
        const Object& object = scene.objects.get(objectIndex);

        Mat4 model = object.transform.getMatrix();
        for (u32 faceIndex = 0; faceIndex < (u32)object.meshData.getFaces().size(); faceIndex++) {
            const Face& face = object.meshData.getFaces()[faceIndex];
        }
    }

    return bestHit;
}