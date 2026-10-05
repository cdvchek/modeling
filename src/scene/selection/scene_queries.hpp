#pragma once

#include <types>
#include <cfloat>

#include "scene/mesh/mesh_handles.hpp"
#include "scene/selection/ray.hpp"
#include "scene/lights/light.hpp"
#include "core/math/mat4.hpp"

struct Scene;

struct VertexHit {
    bool hit = false;

    u32 objectIndex = 0;
    VertexHandle vertex;

    f32 distance = FLT_MAX;
};

VertexHit pickVertex(const Scene& scene, const Ray& ray, f32 radius);

struct EdgeHit {
    bool hit = false;

    u32 objectIndex = 0;
    EdgeHandle edge;

    f32 distance = FLT_MAX;
};

EdgeHit pickEdge(const Scene& scene, const Ray& ray, f32 radius);

struct FaceHit {
    bool hit = false;

    u32 objectIndex = 0;
    FaceHandle face;

    f32 distance = FLT_MAX;
};

FaceHit pickFace(const Scene& scene, const Ray& ray);

struct LightHit {
    bool hit = false;

    LightHandle light = INVALID_LIGHT;

    f32 distance = FLT_MAX;   // pixels from the mouse to the light's marker
};

// Nearest light whose marker is within radius pixels of the mouse
LightHit pickLight(const Scene& scene, const Mat4& viewProjection, f32 mouseX, f32 mouseY, f32 width, f32 height, f32 radius);
