#pragma once

#include <vector>
#include "core/math/mat4.hpp"
#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"
#include "scene/mesh/mesh_data.hpp"

// Making UVs: unwrapping along seams, projections, and packing islands into the texture. Every function works on
// the given faces only; the rest of the mesh's UVs stay as they are. toWorld places the mesh (the object's world
// matrix), so sizes and directions are the ones you see.
namespace UVUnwrap {
    // A rectangle in UV space
    struct Area {
        Vec2 low { 0.0f, 0.0f };
        Vec2 high { 1.0f, 1.0f };
    };

    // The faces split into pieces by seams: faces joined across an edge that isn't a seam, both in the list
    std::vector<std::vector<FaceHandle>> seamIslands(const MeshData& mesh, const std::vector<FaceHandle>& faces);

    // The faces split into the pieces their UVs form (getUVIsland, kept to the list)
    std::vector<std::vector<FaceHandle>> uvIslands(const MeshData& mesh, const std::vector<FaceHandle>& faces);

    // Flattens each seam island with as little distortion of angles as possible (least squares conformal maps),
    // sized to its area in the world and laid the right way round (not mirrored). Islands sit where they land;
    // pack them after. Returns the islands it unwrapped.
    std::vector<std::vector<FaceHandle>> unwrap(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld);

    // Projections: each face's UVs from where its corners are, like a picture shone onto them
    // From a direction: u along right, v down along up (the view's right and up for "from the view")
    void projectPlanar(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld, const Vec3& right, const Vec3& up);
    // Each face from whichever of the six axis directions it faces most
    void projectBox(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld);
    // Wrapped around the object's own up (Y) axis through the faces' middle; u around, v down the height
    void projectCylinder(MeshData& mesh, const std::vector<FaceHandle>& faces);
    // Wrapped around the faces' middle like a globe: u around, v from the top pole to the bottom one
    void projectSphere(MeshData& mesh, const std::vector<FaceHandle>& faces);

    // Arranges the islands inside area without overlapping, all scaled by the same amount so their sizes keep their
    // proportions, as large as fits with margin (a fraction of the area's width) around each. rotate turns tall
    // islands on their side, which usually packs tighter.
    void pack(MeshData& mesh, const std::vector<std::vector<FaceHandle>>& islands, const Area& area, f32 margin, bool rotate);

    // The UV bounds of the faces' corners; false when there are none
    bool bounds(const MeshData& mesh, const std::vector<FaceHandle>& faces, Area& out);
}
