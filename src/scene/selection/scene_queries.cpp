#include "scene/selection/scene_queries.hpp"

#include <cfloat>

#include "scene/scene.hpp"
#include "scene/object_collection.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "core/math/vec4.hpp"

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

        for (u32 vertexIndex = 0; vertexIndex < object.meshData.getVertices().size(); vertexIndex++) {
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