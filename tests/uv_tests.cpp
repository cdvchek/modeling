#include "test.hpp"
#include "mesh_helpers.hpp"
#include "asset/asset_file.hpp"
#include "vlmobj/vlmobj.hpp"

#include <cmath>
#include <set>
#include <utility>

namespace {
    MeshData preset(PresetMesh type) {
        MeshData mesh;
        mesh.setMesh(type);
        return mesh;
    }

    bool sameUV(const Vec2& a, const Vec2& b, f32 tolerance = 1e-5f) {
        return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance;
    }

    // Signed area of a face's UV polygon; its sign says which way the face is laid out
    f32 uvArea(const MeshData& mesh, FaceHandle face) {
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        f32 area = 0.0f;
        for (std::size_t i = 0; i < uvs.size(); ++i) {
            const Vec2& a = uvs[i];
            const Vec2& b = uvs[(i + 1) % uvs.size()];
            area += a.x * b.y - b.x * a.y;
        }
        return area * 0.5f;
    }

    Vec2 uvAt(const MeshData& mesh, FaceHandle face, VertexHandle vertex) {
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        for (std::size_t i = 0; i < vertices.size(); ++i) if (vertices[i] == vertex) return uvs[i];
        return Vec2(-1.0f, -1.0f);
    }

    bool contains(const std::vector<VertexHandle>& vertices, VertexHandle vertex) {
        for (VertexHandle v : vertices) if (v == vertex) return true;
        return false;
    }
}

TEST_CASE(presets_lay_out_uvs_in_range_and_one_way_round) {
    for (PresetMesh type : { PresetMesh::Cube, PresetMesh::Plane, PresetMesh::Grid, PresetMesh::Circle, PresetMesh::Cylinder,
                             PresetMesh::Cone, PresetMesh::UVSphere, PresetMesh::Torus }) {
        const MeshData mesh = preset(type);
        bool inRange = true;
        for (const Vec2& uv : mesh.getCornerUVs()) inRange = inRange && uv.x >= -1e-5f && uv.x <= 1.0f + 1e-5f && uv.y >= -1e-5f && uv.y <= 1.0f + 1e-5f;
        CHECK(inRange);

        // Every face has area in UV space, all laid out the same way round
        int positive = 0, negative = 0;
        for (FaceHandle face : mesh.getFaceHandles()) {
            const f32 area = uvArea(mesh, face);
            if (area > 1e-7f) ++positive;
            else if (area < -1e-7f) ++negative;
        }
        CHECK(positive + negative == static_cast<int>(mesh.getFaceHandles().size()));
        CHECK(positive == 0 || negative == 0);
    }

    // The ico sphere wraps around a seam, so u may run a little past 1; it still has area everywhere
    const MeshData ico = preset(PresetMesh::IcoSphere);
    bool finite = true;
    for (const Vec2& uv : ico.getCornerUVs()) finite = finite && std::isfinite(uv.x) && std::isfinite(uv.y);
    CHECK(finite);
}

TEST_CASE(cube_uv_cells_dont_overlap) {
    const MeshData mesh = cube();
    std::set<std::pair<int, int>> cells;
    for (FaceHandle face : mesh.getFaceHandles()) {
        Vec2 centre(0.0f, 0.0f);
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        for (const Vec2& uv : uvs) centre = centre + uv;
        centre = centre / static_cast<f32>(uvs.size());
        cells.insert({ static_cast<int>(centre.x * 4.0f), static_cast<int>(centre.y * 4.0f) });
    }
    CHECK(cells.size() == 6);
}

TEST_CASE(extrude_keeps_the_top_uvs_and_gives_sides_the_rim) {
    MeshData mesh = cube();
    const FaceHandle top = mesh.getFaceHandles()[0];
    const std::vector<Vec2> before = mesh.getFaceUVs(top);

    extrude(mesh, top);
    CHECK(mesh.validate());

    // The top keeps its corners' UVs
    const std::vector<Vec2> after = mesh.getFaceUVs(top);
    bool sameTop = after.size() == before.size();
    for (std::size_t i = 0; sameTop && i < before.size(); ++i) sameTop = sameUV(before[i], after[i]);
    CHECK(sameTop);

    // Each new side face's corners take UVs from the old rim
    bool fromRim = true;
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    for (std::size_t f = 6; f < faces.size(); ++f) {
        for (const Vec2& uv : mesh.getFaceUVs(faces[f])) {
            bool found = false;
            for (const Vec2& rim : before) found = found || sameUV(uv, rim);
            fromRim = fromRim && found;
        }
    }
    CHECK(faces.size() == 10);
    CHECK(fromRim);
}

TEST_CASE(split_edge_puts_the_midpoint_uv_on_each_side) {
    MeshData mesh = cube();
    const EdgeHandle edge = mesh.getEdgeHandles()[0];
    const VertexHandle origin = mesh.getEdgeOrigin(edge), tip = mesh.getEdgeTip(edge);

    std::vector<std::pair<FaceHandle, Vec2>> expected;
    for (FaceHandle face : mesh.getFaceHandles()) {
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
        if (contains(vertices, origin) && contains(vertices, tip)) expected.push_back({ face, (uvAt(mesh, face, origin) + uvAt(mesh, face, tip)) * 0.5f });
    }
    CHECK(expected.size() == 2);

    const VertexHandle middle = mesh.splitEdge(edge);
    CHECK(mesh.validate());
    bool midpoints = true;
    for (const auto& [face, uv] : expected) midpoints = midpoints && sameUV(uvAt(mesh, face, middle), uv);
    CHECK(midpoints);
}

TEST_CASE(connect_keeps_each_corner_uv) {
    MeshData mesh = cube();
    const FaceHandle face = mesh.getFaceHandles()[0];
    const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
    const std::vector<Vec2> uvs = mesh.getFaceUVs(face);

    CHECK(mesh.connectVertices(corners[0], corners[2]));
    CHECK(mesh.validate());

    // Both halves of the old face carry its UVs at the corners they share with it
    int checked = 0;
    bool same = true;
    for (FaceHandle f : mesh.getFaceHandles()) {
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(f);
        if (vertices.size() != 3) continue;
        for (std::size_t i = 0; i < corners.size(); ++i) {
            if (!contains(vertices, corners[i])) continue;
            same = same && sameUV(uvAt(mesh, f, corners[i]), uvs[i]);
            ++checked;
        }
    }
    CHECK(checked == 6);
    CHECK(same);
}

TEST_CASE(bevel_keeps_uvs_on_untouched_faces) {
    MeshData mesh = cube();
    const VertexHandle corner = mesh.getVertexHandles()[0];

    // Faces away from the corner and their UVs
    std::vector<std::pair<FaceHandle, std::vector<Vec2>>> away;
    for (FaceHandle face : mesh.getFaceHandles()) {
        if (!contains(mesh.getFaceVertices(face), corner)) away.push_back({ face, mesh.getFaceUVs(face) });
    }
    CHECK(away.size() == 3);

    SlideSession session;
    CHECK(mesh.bevelVertex(corner, session));
    mesh.setSlideWidth(session, 0.25f);
    CHECK(mesh.validate());

    bool kept = true;
    for (const auto& [face, uvs] : away) {
        const std::vector<Vec2> now = mesh.getFaceUVs(face);
        kept = kept && now.size() == uvs.size();
        for (std::size_t i = 0; kept && i < uvs.size(); ++i) kept = sameUV(now[i], uvs[i]);
    }
    CHECK(kept);

    // Every UV still lies on the cube's layout
    bool inRange = true;
    for (const Vec2& uv : mesh.getCornerUVs()) inRange = inRange && uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f;
    CHECK(inRange);
}

TEST_CASE(asset_round_trip_keeps_uvs) {
    Object object;
    object.name = "Crate";
    object.meshData.setMesh(PresetMesh::Cube);
    const FaceHandle face = object.meshData.getFaceHandles()[3];
    object.meshData.setFaceUVs(face, { Vec2(0.1f, 0.2f), Vec2(0.9f, 0.2f), Vec2(0.9f, 0.7f), Vec2(0.1f, 0.7f) });

    Object loaded;
    std::string error;
    CHECK(AssetFile::read(AssetFile::write(object), loaded, error));

    // Face by face: the corner at each position keeps its UV (a loop may start at a different corner)
    const std::vector<FaceHandle> a = object.meshData.getFaceHandles();
    const std::vector<FaceHandle> b = loaded.meshData.getFaceHandles();
    bool same = a.size() == b.size();
    for (std::size_t f = 0; same && f < a.size(); ++f) {
        const std::vector<VertexHandle> va = object.meshData.getFaceVertices(a[f]);
        const std::vector<VertexHandle> vb = loaded.meshData.getFaceVertices(b[f]);
        const std::vector<Vec2> ua = object.meshData.getFaceUVs(a[f]);
        const std::vector<Vec2> ub = loaded.meshData.getFaceUVs(b[f]);
        same = va.size() == vb.size();
        for (std::size_t i = 0; same && i < va.size(); ++i) {
            const Vec3 p = object.meshData.getVertexPosition(va[i]);
            bool matched = false;
            for (std::size_t k = 0; k < vb.size(); ++k) {
                const Vec3 q = loaded.meshData.getVertexPosition(vb[k]);
                if (p.x == q.x && p.y == q.y && p.z == q.z) matched = ua[i].x == ub[k].x && ua[i].y == ub[k].y;
            }
            same = matched;
        }
    }
    CHECK(same);
}

TEST_CASE(asset_bake_writes_a_uv_attribute_and_splits_at_seams) {
    Object object;
    object.name = "Box";
    object.meshData.setMesh(PresetMesh::Cube);
    const std::vector<u8> bytes = AssetFile::write(object);

    vlmobj::File file;
    CHECK(file.open(bytes.data(), bytes.size()));
    const vlmobj::Mesh& mesh = file.meshes()[0];
    bool hasUV = false;
    for (u32 a = 0; a < mesh.attributeCount; ++a) {
        const vlmobj::VertexAttribute& attribute = mesh.attributes[a];
        if (attribute.semantic == static_cast<u8>(vlmobj::Semantic::UV0)) {
            hasUV = attribute.format == static_cast<u8>(vlmobj::Format::F32x2) && attribute.offset == 24;
        }
    }
    CHECK(hasUV);
    CHECK(mesh.vertexStride == 32);

    // Each corner keeps its UV: the 24 baked corners hold every distinct corner UV of the cube
    const AssetFile::BakedMesh baked = AssetFile::bake(object.meshData);
    std::set<std::pair<f32, f32>> bakedUVs, meshUVs;
    for (std::size_t v = 0; v < baked.vertices.size(); v += 8) bakedUVs.insert({ baked.vertices[v + 6], baked.vertices[v + 7] });
    for (const Vec2& uv : object.meshData.getCornerUVs()) meshUVs.insert({ uv.x == 0.0f ? 0.0f : uv.x, uv.y == 0.0f ? 0.0f : uv.y });
    CHECK(bakedUVs == meshUVs);
}
