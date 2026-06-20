#include "scene/selection/ray.hpp"

#include "scene/camera.hpp"
#include "core/math/vec4.hpp"
#include "core/math/mat4.hpp"

Ray makeRayFromScreenPosition(
    i32 mouseX,
    i32 mouseY,
    u32 width,
    u32 height,
    const Camera& camera
) {
    f32 x = (2.0f * static_cast<f32>(mouseX)) / static_cast<f32>(width) - 1.0f;
    f32 y = 1.0f - (2.0f * static_cast<f32>(mouseY)) / static_cast<f32>(height);

    Vec4 nearClip(x, y, -1.0f, 1.0f);
    Vec4 farClip (x, y,  1.0f, 1.0f);

    f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

    Mat4 view = camera.getViewMatrix();
    Mat4 projection = camera.getProjectionMatrix(aspectRatio);

    Mat4 inverseViewProjection = Mat4::inverse(projection * view);

    Vec4 nearWorld = inverseViewProjection * nearClip;
    Vec4 farWorld  = inverseViewProjection * farClip;

    nearWorld = nearWorld / nearWorld.w;
    farWorld  = farWorld / farWorld.w;

    Vec3 origin(
        nearWorld.x,
        nearWorld.y,
        nearWorld.z
    );

    Vec3 farPoint(
        farWorld.x,
        farWorld.y,
        farWorld.z
    );

    Ray ray;
    ray.origin = origin;
    ray.direction = (farPoint - origin).normalized();

    return ray;
}