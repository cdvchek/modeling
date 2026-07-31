#pragma once

#include <types>

#include "scene/selection/ray.hpp"

struct Scene;

struct VertexHit {
    bool hit = false;

    u32 objectIndex = 0;
    u32 vertexIndex = 0;

    f32 distance = 0.0f;
};

VertexHit pickVertex(
    const Scene& scene,
    const Ray& ray,
    f32 radius
);

struct FaceHit {
    bool hit = false;

    u32 objectIndex = 0;
    u32 faceIndex = 0;

    f32 distance = 0.0f;
};

FaceHit pickFace(
    const Scene& scene,
    const Ray& ray,
    f32 radius
);