#pragma once

#include <vector>
#include "scene/mesh/mesh_types.hpp"

namespace MeshFactory {
    // Faces list vertex indices counter-clockwise seen from outside; open edges get border half-edges
    PackagedMesh fromPolygons(const std::vector<Vec3>& positions, const std::vector<std::vector<u32>>& faces);

    PackagedMesh cube();
    PackagedMesh plane(f32 size = 1.0f, u32 divisions = 1);
    PackagedMesh grid(f32 size = 2.0f, u32 divisions = 10);
    PackagedMesh circle(f32 radius = 0.5f, u32 sides = 32);
    PackagedMesh cylinder(f32 radius = 0.5f, f32 height = 1.0f, u32 sides = 16);
    PackagedMesh cone(f32 radius = 0.5f, f32 height = 1.0f, u32 sides = 16);
    PackagedMesh uvSphere(f32 radius = 0.5f, u32 segments = 16, u32 rings = 8);
    PackagedMesh icoSphere(f32 radius = 0.5f, u32 subdivisions = 1);
    PackagedMesh torus(f32 majorRadius = 0.4f, f32 minorRadius = 0.15f, u32 segments = 24, u32 sides = 12);
}
