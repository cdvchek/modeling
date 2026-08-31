#pragma once

#include <types>
#include <cfloat>

#include "scene/mesh/mesh_array.hpp"
#include "scene/selection/ray.hpp"

struct Scene;

struct VertexHit {
    bool hit = false;

    u32 objectIndex = 0;
    VertexHandle vertex;

    f32 distance = FLT_MAX;
};

VertexHit pickVertex(const Scene& scene, const Ray& ray, f32 radius);

struct FaceHit {
    bool hit = false;

    u32 objectIndex = 0;
    FaceHandle face;

    f32 distance = FLT_MAX;
};

FaceHit pickFace(const Scene& scene, const Ray& ray);