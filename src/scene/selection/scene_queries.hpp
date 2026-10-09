#pragma once

#include <types>
#include <cfloat>
#include <functional>

#include "scene/mesh/mesh_handles.hpp"
#include "scene/selection/ray.hpp"
#include "scene/lights/light.hpp"
#include "scene/references/reference_image.hpp"
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

// Whether an object's back faces aren't drawn, so they can't be clicked either
using BackFacesCulled = std::function<bool(ObjectHandle)>;

// only limits the test to one object; exclude skips one (used to find another object under the mouse).
// With culled, faces seen from behind on objects it names are skipped, as the GPU skips drawing them.
FaceHit pickFace(const Scene& scene, const Ray& ray, ObjectHandle only = INVALID_OBJECT, ObjectHandle exclude = INVALID_OBJECT,
                 const BackFacesCulled& culled = {});

struct LightHit {
    bool hit = false;

    LightHandle light = INVALID_LIGHT;

    f32 distance = FLT_MAX;   // pixels from the mouse to the light's marker
};

struct OriginHit {
    bool hit = false;

    ObjectHandle object = INVALID_OBJECT;

    f32 distance = FLT_MAX;   // pixels from the mouse to the origin's marker
};

// Nearest object origin within radius pixels of the mouse
OriginHit pickOrigin(const Scene& scene, const Mat4& viewProjection, f32 mouseX, f32 mouseY, f32 width, f32 height, f32 radius);

// Nearest light whose marker is within radius pixels of the mouse
LightHit pickLight(const Scene& scene, const Mat4& viewProjection, f32 mouseX, f32 mouseY, f32 width, f32 height, f32 radius);

struct ReferenceHit {
    bool hit = false;

    ReferenceHandle reference = INVALID_REFERENCE;
    ReferenceDepth depth = ReferenceDepth::InScene;

    f32 distance = FLT_MAX;   // along the ray, in world units
};

// The reference image the ray meets first, skipping hidden and locked ones. Images drawn in front of everything
// win over the rest, and images drawn behind everything lose to the rest, whatever their distance.
ReferenceHit pickReference(const Scene& scene, const Ray& ray);
