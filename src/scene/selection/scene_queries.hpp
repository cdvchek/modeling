#pragma once

#include <types>
#include <cfloat>

#include "scene/mesh/mesh_handles.hpp"
#include "scene/selection/ray.hpp"
#include "scene/lights/light.hpp"
#include "scene/objects/object_collection.hpp"
#include "core/math/mat4.hpp"

struct Scene;

struct VertexHit {
    bool hit = false;

    ObjectHandle object = INVALID_OBJECT;
    VertexHandle vertex;

    f32 distance = FLT_MAX;
};

// Only the given object is tested (INVALID_OBJECT tests all)
VertexHit pickVertex(const Scene& scene, const Ray& ray, f32 radius, ObjectHandle only = INVALID_OBJECT);

struct EdgeHit {
    bool hit = false;

    ObjectHandle object = INVALID_OBJECT;
    EdgeHandle edge;

    f32 distance = FLT_MAX;
};

EdgeHit pickEdge(const Scene& scene, const Ray& ray, f32 radius, ObjectHandle only = INVALID_OBJECT);

struct FaceHit {
    bool hit = false;

    ObjectHandle object = INVALID_OBJECT;
    FaceHandle face;

    f32 distance = FLT_MAX;
};

// only limits the test to one object; exclude skips one (used to find another object under the mouse)
FaceHit pickFace(const Scene& scene, const Ray& ray, ObjectHandle only = INVALID_OBJECT, ObjectHandle exclude = INVALID_OBJECT);

struct LightHit {
    bool hit = false;

    LightHandle light = INVALID_LIGHT;

    f32 distance = FLT_MAX;   // pixels from the mouse to the light's marker
};

// Nearest light whose marker is within radius pixels of the mouse
LightHit pickLight(const Scene& scene, const Mat4& viewProjection, f32 mouseX, f32 mouseY, f32 width, f32 height, f32 radius);
