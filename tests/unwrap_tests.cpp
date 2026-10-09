#include "test.hpp"
#include "mesh_helpers.hpp"
#include "asset/asset_file.hpp"
#include "project/project_file.hpp"
#include "scene/mesh/uv_unwrap.hpp"

#include <cmath>

namespace {
    MeshData preset(PresetMesh type) {
        MeshData mesh;
        mesh.setMesh(type);
        return mesh;
    }

    f32 triangleArea(Vec2 a, Vec2 b, Vec2 c) {
        return ((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
    }

    // Signed UV area of a face, by its triangles
    f32 faceUVArea(const MeshData& mesh, FaceHandle face) {
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        f32 area = 0.0f;
        for (std::size_t i = 1; i + 1 < uvs.size(); ++i) area += triangleArea(uvs[0], uvs[i], uvs[i + 1]);
        return area;
    }

    bool allSameWay(const MeshData& mesh, const std::vector<FaceHandle>& faces, f32 sign) {
        for (FaceHandle face : faces) if (faceUVArea(mesh, face) * sign <= 0.0f) return false;
        return true;
    }

    // The way round the presets lay faces out (the plane's one face)
    f32 presetSign() {
        const MeshData plane = preset(PresetMesh::Plane);
        return faceUVArea(plane, plane.getFaceHandles()[0]) > 0.0f ? 1.0f : -1.0f;
    }

    bool inUnitSquare(const MeshData& mesh) {
        for (const Vec2& uv : mesh.getCornerUVs()) {
            if (uv.x < -1e-4f || uv.x > 1.0f + 1e-4f || uv.y < -1e-4f || uv.y > 1.0f + 1e-4f) return false;
        }
        return true;
    }

    bool overlap(const UVUnwrap::Area& a, const UVUnwrap::Area& b) {
        return a.low.x < b.high.x - 1e-4f && b.low.x < a.high.x - 1e-4f && a.low.y < b.high.y - 1e-4f && b.low.y < a.high.y - 1e-4f;
    }

    std::vector<FaceHandle> allFaces(const MeshData& mesh) {
        return mesh.getFaceHandles();
    }
}

TEST_CASE(seams_are_marked_on_both_halves_and_survive_edits) {
    MeshData mesh = cube();
    const EdgeHandle edge = mesh.getEdgeHandles()[0];
    mesh.setSeam(edge, true);
    CHECK(mesh.isSeam(edge) && mesh.isSeam(mesh.getEdge(edge)->pair));
    CHECK(mesh.getSeamEdges().size() == 1);
    CHECK(mesh.hasSeams());

    // Both pieces of a split edge stay seams
    mesh.splitEdge(edge);
    CHECK(mesh.validate());
    CHECK(mesh.getSeamEdges().size() == 2);

    // A bevelled corner keeps the seams of edges that still join the same vertices
    MeshData beveled = cube();
    for (EdgeHandle e : beveled.getEdgeHandles()) beveled.setSeam(e, true);
    SlideSession session;
    CHECK(beveled.bevelVertex(beveled.getVertexHandles()[0], session));
    beveled.setSlideWidth(session, 0.25f);
    CHECK(beveled.validate());
    CHECK(beveled.getSeamEdges().size() == 9);

    mesh.setSeam(edge, false);
    CHECK(mesh.getSeamEdges().size() == 1);
}

TEST_CASE(seams_from_islands_cut_where_the_layout_is_cut) {
    // The cube's cross keeps 5 of its 12 edges joined
    MeshData box = cube();
    CHECK(box.markSeamsFromIslands() == 7);
    CHECK(UVUnwrap::seamIslands(box, allFaces(box)).size() == 1);

    // The cylinder: its side's one vertical cut, and both cap rims
    MeshData cylinder = preset(PresetMesh::Cylinder);
    CHECK(cylinder.markSeamsFromIslands() == 33);
    CHECK(UVUnwrap::seamIslands(cylinder, allFaces(cylinder)).size() == 3);

    // Every edge a seam: every face its own piece
    MeshData cut = cube();
    for (EdgeHandle edge : cut.getEdgeHandles()) cut.setSeam(edge, true);
    CHECK(UVUnwrap::seamIslands(cut, allFaces(cut)).size() == 6);
}

TEST_CASE(unwrap_flattens_without_distorting_angles) {
    // A flat grid comes out as squares, its real size, laid the presets' way round
    MeshData grid = preset(PresetMesh::Grid);
    const auto islands = UVUnwrap::unwrap(grid, allFaces(grid), Mat4::identity());
    CHECK(islands.size() == 1);
    CHECK(allSameWay(grid, allFaces(grid), presetSign()));

    bool squares = true;
    f32 total = 0.0f;
    for (FaceHandle face : grid.getFaceHandles()) {
        const std::vector<Vec2> uvs = grid.getFaceUVs(face);
        const f32 side = (uvs[1] - uvs[0]).length();
        squares = squares && std::fabs(side - 0.2f) < 1e-3f && std::fabs((uvs[2] - uvs[1]).length() - side) < 1e-3f
            && std::fabs(Vec2::dot(uvs[1] - uvs[0], uvs[2] - uvs[1])) < 1e-4f;
        total += std::fabs(faceUVArea(grid, face));
    }
    CHECK(squares);
    CHECK(std::fabs(total - 4.0f) < 1e-2f);

    // The cylinder's side, cut once, opens into a strip with no face flipped
    MeshData cylinder = preset(PresetMesh::Cylinder);
    cylinder.markSeamsFromIslands();
    const auto pieces = UVUnwrap::unwrap(cylinder, allFaces(cylinder), Mat4::identity());
    CHECK(pieces.size() == 3);
    CHECK(allSameWay(cylinder, allFaces(cylinder), presetSign()));
}

TEST_CASE(unwrap_opens_a_cut_closed_surface) {
    // The cube cut along its cross: one island, each side a unit square, none flipped, total area 6
    MeshData box = cube();
    box.markSeamsFromIslands();
    const auto islands = UVUnwrap::unwrap(box, allFaces(box), Mat4::identity());
    CHECK(islands.size() == 1);
    CHECK(allSameWay(box, allFaces(box), presetSign()));

    f32 total = 0.0f;
    bool squares = true;
    for (FaceHandle face : box.getFaceHandles()) {
        const f32 area = std::fabs(faceUVArea(box, face));
        total += area;
        squares = squares && std::fabs(area - 1.0f) < 1e-2f;
    }
    CHECK(squares);
    CHECK(std::fabs(total - 6.0f) < 5e-2f);
}

TEST_CASE(unwrap_touches_only_the_given_faces) {
    MeshData grid = preset(PresetMesh::Grid);
    const std::vector<FaceHandle> faces = grid.getFaceHandles();
    const std::vector<Vec2> untouched = grid.getFaceUVs(faces[90]);

    UVUnwrap::unwrap(grid, { faces[0], faces[1] }, Mat4::identity());
    const std::vector<Vec2> after = grid.getFaceUVs(faces[90]);
    bool same = true;
    for (std::size_t i = 0; i < untouched.size(); ++i) same = same && untouched[i].x == after[i].x && untouched[i].y == after[i].y;
    CHECK(same);
}

TEST_CASE(pack_fits_islands_inside_without_overlap) {
    MeshData box = cube();
    for (EdgeHandle edge : box.getEdgeHandles()) box.setSeam(edge, true);
    const auto islands = UVUnwrap::unwrap(box, allFaces(box), Mat4::identity());
    CHECK(islands.size() == 6);

    UVUnwrap::pack(box, islands, UVUnwrap::Area {}, 0.02f, true);
    CHECK(inUnitSquare(box));
    CHECK(allSameWay(box, allFaces(box), presetSign()));

    // No two islands overlap, and equal faces stay equal in size
    std::vector<UVUnwrap::Area> boxes;
    for (const auto& island : islands) {
        UVUnwrap::Area area;
        UVUnwrap::bounds(box, island, area);
        boxes.push_back(area);
    }
    bool apart = true, equal = true;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) apart = apart && !overlap(boxes[i], boxes[j]);
        equal = equal && std::fabs((boxes[i].high.x - boxes[i].low.x) - (boxes[0].high.x - boxes[0].low.x)) < 1e-3f;
    }
    CHECK(apart);
    CHECK(equal);

    // Into a smaller area too
    UVUnwrap::pack(box, islands, UVUnwrap::Area { Vec2(0.5f, 0.5f), Vec2(1.0f, 0.75f) }, 0.0f, false);
    UVUnwrap::Area all;
    UVUnwrap::bounds(box, allFaces(box), all);
    CHECK(all.low.x >= 0.5f - 1e-4f && all.high.x <= 1.0f + 1e-4f && all.low.y >= 0.5f - 1e-4f && all.high.y <= 0.75f + 1e-4f);
}

TEST_CASE(projections_lay_faces_the_right_way_round) {
    MeshData box = cube();
    UVUnwrap::projectBox(box, allFaces(box), Mat4::identity());
    CHECK(allSameWay(box, allFaces(box), presetSign()));
    // Each side faces its own way, so each is its own piece
    CHECK(UVUnwrap::uvIslands(box, allFaces(box)).size() == 6);

    MeshData cylinder = preset(PresetMesh::Cylinder);
    std::vector<FaceHandle> sides;
    for (FaceHandle face : cylinder.getFaceHandles()) if (cylinder.getFaceVertices(face).size() == 4) sides.push_back(face);
    UVUnwrap::projectCylinder(cylinder, sides);
    CHECK(allSameWay(cylinder, sides, presetSign()));
    CHECK(UVUnwrap::uvIslands(cylinder, sides).size() == 1);

    MeshData sphere = preset(PresetMesh::UVSphere);
    UVUnwrap::projectSphere(sphere, allFaces(sphere));
    CHECK(allSameWay(sphere, allFaces(sphere), presetSign()));

    // From the front: u along +X, v down along +Y
    MeshData plane = preset(PresetMesh::Plane);
    UVUnwrap::projectPlanar(plane, allFaces(plane), Mat4::identity(), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    CHECK(allSameWay(plane, allFaces(plane), presetSign()));
}

TEST_CASE(seams_are_saved_and_exported) {
    Scene scene;
    const ObjectHandle handle = scene.objects.add("Crate", PresetMesh::Cube);
    scene.objects.get(handle).meshData.markSeamsFromIslands();

    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(scene, ProjectFile::View()), loaded, view, error));
    CHECK(loaded.objects.get(loaded.objects.handleAt(0)).meshData.getSeamEdges().size() == 7);

    Object object;
    CHECK(AssetFile::read(AssetFile::write(scene.objects.get(handle)), object, error));
    CHECK(object.meshData.getSeamEdges().size() == 7);
}
