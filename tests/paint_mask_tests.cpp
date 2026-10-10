#include "test.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "scene/textures/brush.hpp"
#include "scene/textures/paint_mask.hpp"
#include "scene/textures/paint_targets.hpp"

#include <algorithm>

namespace {
    // A cube whose first face covers the UV rectangle low to high and whose other faces sit in a far corner
    MeshData cubeWithFace(Vec2 low, Vec2 high, FaceHandle& face) {
        MeshData mesh;
        mesh.setMesh(PresetMesh::Cube);
        const std::vector<FaceHandle> faces = mesh.getFaceHandles();
        for (FaceHandle other : faces) mesh.setFaceUVs(other, { Vec2(0.9f, 0.9f), Vec2(0.95f, 0.9f), Vec2(0.95f, 0.95f), Vec2(0.9f, 0.95f) });
        face = faces[0];
        mesh.setFaceUVs(face, { low, Vec2(high.x, low.y), high, Vec2(low.x, high.y) });
        return mesh;
    }

    std::size_t marked(const PaintMask& mask) {
        return static_cast<std::size_t>(std::count(mask.pixels.begin(), mask.pixels.end(), u8(255)));
    }
}

TEST_CASE(mask_covers_a_face_and_a_pixel_past_it) {
    // The face covers pixels 16 to 47 each way on a 64 x 64 texture
    FaceHandle face;
    const MeshData mesh = cubeWithFace(Vec2(0.25f, 0.25f), Vec2(0.75f, 0.75f), face);

    // With no reach: exactly its pixels
    const PaintMask exact = maskFromFaces(mesh, { face }, 64, 64, 0.0f);
    CHECK(exact.width == 64 && exact.height == 64 && exact.pixels.size() == 64 * 64);
    CHECK(marked(exact) == 32 * 32);
    CHECK(exact.at(16, 16) == 255 && exact.at(47, 47) == 255 && exact.at(30, 20) == 255);
    CHECK(exact.at(15, 30) == 0 && exact.at(48, 30) == 0 && exact.at(30, 15) == 0 && exact.at(30, 48) == 0);

    // By default one pixel further on every side, and nothing of the other faces
    const PaintMask mask = maskFromFaces(mesh, { face }, 64, 64);
    CHECK(mask.at(15, 30) == 255 && mask.at(48, 30) == 255 && mask.at(30, 15) == 255 && mask.at(30, 48) == 255);
    CHECK(mask.at(14, 30) == 0 && mask.at(49, 30) == 0 && mask.at(30, 14) == 0 && mask.at(30, 49) == 0);
    CHECK(mask.at(59, 59) == 0);
    CHECK(marked(mask) > 32 * 32 && marked(mask) <= 34 * 34);

    // Two faces mark both; no faces mark nothing
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    const PaintMask both = maskFromFaces(mesh, { face, faces[1] }, 64, 64, 0.0f);
    CHECK(both.at(30, 30) == 255 && both.at(59, 59) == 255 && both.at(52, 52) == 0);
    CHECK(marked(maskFromFaces(mesh, {}, 64, 64)) == 0);
}

TEST_CASE(mask_follows_slanted_mirrored_and_repeated_uvs) {
    FaceHandle face;
    MeshData mesh = cubeWithFace(Vec2(0.25f, 0.25f), Vec2(0.75f, 0.75f), face);

    // A triangle's worth of UVs (two corners together): the slanted edge runs from the top right to the bottom left
    mesh.setFaceUVs(face, { Vec2(0.25f, 0.25f), Vec2(0.75f, 0.25f), Vec2(0.25f, 0.75f), Vec2(0.25f, 0.75f) });
    const PaintMask slanted = maskFromFaces(mesh, { face }, 64, 64, 0.0f);
    CHECK(slanted.at(18, 18) == 255 && slanted.at(40, 20) == 255 && slanted.at(20, 40) == 255);
    CHECK(slanted.at(40, 40) == 0 && slanted.at(46, 46) == 0);
    CHECK(marked(slanted) > 450 && marked(slanted) < 580);

    // Mirrored (the corners the other way round) marks the same pixels
    mesh.setFaceUVs(face, { Vec2(0.25f, 0.75f), Vec2(0.25f, 0.75f), Vec2(0.75f, 0.25f), Vec2(0.25f, 0.25f) });
    CHECK(maskFromFaces(mesh, { face }, 64, 64, 0.0f).pixels == slanted.pixels);

    // Past 1 and below 0 the texture repeats, so the same pixels again
    const PaintMask square = maskFromFaces(cubeWithFace(Vec2(0.25f, 0.25f), Vec2(0.75f, 0.75f), face), { face }, 64, 64, 0.0f);
    CHECK(maskFromFaces(cubeWithFace(Vec2(1.25f, 2.25f), Vec2(1.75f, 2.75f), face), { face }, 64, 64, 0.0f).pixels == square.pixels);
    CHECK(maskFromFaces(cubeWithFace(Vec2(-0.75f, -1.75f), Vec2(-0.25f, -1.25f), face), { face }, 64, 64, 0.0f).pixels == square.pixels);

    // Across the edge of the picture: both ends of it
    const PaintMask across = maskFromFaces(cubeWithFace(Vec2(0.75f, 0.25f), Vec2(1.25f, 0.75f), face), { face }, 64, 64, 0.0f);
    CHECK(across.at(60, 30) == 255 && across.at(4, 30) == 255 && across.at(30, 30) == 0);

    // A face squashed to a line has no inside: only what's within reach of the line
    const PaintMask line = maskFromFaces(cubeWithFace(Vec2(0.25f, 0.5f), Vec2(0.75f, 0.5f), face), { face }, 64, 64);
    CHECK(line.at(30, 31) == 255 && line.at(30, 32) == 255 && line.at(30, 29) == 0 && line.at(30, 34) == 0);
    CHECK(marked(maskFromFaces(cubeWithFace(Vec2(0.25f, 0.5f), Vec2(0.75f, 0.5f), face), { face }, 64, 64, 0.0f)) == 0);
}

TEST_CASE(mask_of_an_island_leaves_the_others_out) {
    // The cylinder is laid out as its side and two caps, each an island
    MeshData mesh;
    mesh.setMesh(PresetMesh::Cylinder);
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    const auto middle = [&](FaceHandle face) {
        Vec2 sum;
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        for (const Vec2& uv : uvs) sum = sum + uv;
        return texturePoint(sum / static_cast<f32>(uvs.size()), 256, 256);
    };

    // A side face has four corners; the caps have many
    FaceHandle side;
    for (FaceHandle face : faces) if (mesh.getFaceVertices(face).size() == 4) side = face;
    const std::vector<FaceHandle> island = mesh.getUVIsland(side);
    CHECK(island.size() > 1 && island.size() < faces.size());
    const PaintMask mask = maskFromFaces(mesh, island, 256, 256);

    bool sidesIn = true, capsOut = true;
    for (FaceHandle face : faces) {
        const Vec2 at = middle(face);
        const bool inMask = mask.at(static_cast<u32>(at.x), static_cast<u32>(at.y)) == 255;
        if (std::find(island.begin(), island.end(), face) != island.end()) sidesIn = sidesIn && inMask;
        else capsOut = capsOut && !inMask;
    }
    CHECK(sidesIn);
    CHECK(capsOut);
}

TEST_CASE(stroke_keeps_to_its_mask) {
    // The mask is the left half of a 128 x 128 texture, with no reach
    FaceHandle face;
    const MeshData mesh = cubeWithFace(Vec2(0.0f, 0.0f), Vec2(0.5f, 1.0f), face);
    const auto left = std::make_shared<const PaintMask>(maskFromFaces(mesh, { face }, 128, 128, 0.0f));
    const auto right = std::make_shared<const PaintMask>(maskFromFaces(cubeWithFace(Vec2(0.5f, 0.0f), Vec2(0.875f, 1.0f), face), { face }, 128, 128, 0.0f));

    LayerStack stack = solidLayers(128, 128, Vec3(1.0f, 1.0f, 1.0f));
    const u32 paint = addLayer(stack);
    Brush brush;
    brush.color = Vec3(1.0f, 0.0f, 0.0f);
    brush.size = 20.0f;
    brush.softness = 0.0f;
    const auto alpha = [&](u32 x, u32 y) { return layerPixel(stack, paint, x, y)[3]; };

    // A dab on the mask's edge paints its inside half only
    Stroke stroke;
    stroke.begin(stack, paint, brush);
    stroke.setMask(left);
    CHECK(stroke.moveTo(stack, Vec2(64.0f, 30.0f)));
    CHECK(alpha(60, 30) == 255 && alpha(63, 30) == 255);
    CHECK(alpha(64, 30) == 0 && alpha(70, 30) == 0);

    // Dragged across, the line stops at the edge; the other mask lets the same stroke go on from there
    stroke.moveTo(stack, Vec2(20.0f, 80.0f));
    stroke.moveTo(stack, Vec2(110.0f, 80.0f));
    CHECK(alpha(40, 80) == 255 && alpha(63, 80) == 255 && alpha(64, 80) == 0 && alpha(100, 80) == 0);
    stroke.setMask(right);
    stroke.moveTo(stack, Vec2(60.0f, 80.0f));
    CHECK(alpha(64, 80) == 255 && alpha(100, 80) == 255);
    CHECK(alpha(112, 80) == 0);

    // Without a mask it paints anywhere again; ending the stroke drops the mask for the next one
    stroke.setMask(nullptr);
    stroke.moveTo(stack, Vec2(110.0f, 20.0f));
    CHECK(alpha(110, 20) == 255);
    stroke.setMask(left);
    stroke.end();
    stroke.begin(stack, paint, brush);
    CHECK(stroke.moveTo(stack, Vec2(100.0f, 110.0f)));
    CHECK(alpha(100, 110) == 255);

    // A mask that isn't the texture's size lets nothing through
    stroke.begin(stack, paint, brush);
    stroke.setMask(std::make_shared<const PaintMask>(maskFromFaces(mesh, { face }, 64, 64)));
    CHECK(!stroke.moveTo(stack, Vec2(30.0f, 110.0f)));
    CHECK(alpha(30, 110) == 0);
}
