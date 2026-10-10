#include "scene/textures/paint_mask.hpp"

#include <algorithm>
#include <cmath>

namespace {
    f32 cross(Vec2 a, Vec2 b) {
        return a.x * b.y - a.y * b.x;
    }

    // How far a point is from the stretch between two others, squared
    f32 distanceSquared(Vec2 point, Vec2 from, Vec2 to) {
        const Vec2 along = to - from;
        const f32 length = along.x * along.x + along.y * along.y;
        const Vec2 toPoint = point - from;
        const f32 t = length > 0.0f ? std::clamp((toPoint.x * along.x + toPoint.y * along.y) / length, 0.0f, 1.0f) : 0.0f;
        const Vec2 away = toPoint - along * t;
        return away.x * away.x + away.y * away.y;
    }

    // Marks every pixel whose middle is in the triangle (given in pixels) or within reach of it
    void markTriangle(PaintMask& mask, Vec2 a, Vec2 b, Vec2 c, f32 reach) {
        const f32 left = std::floor(std::min({ a.x, b.x, c.x }) - reach), top = std::floor(std::min({ a.y, b.y, c.y }) - reach);
        const f32 right = std::ceil(std::max({ a.x, b.x, c.x }) + reach), bottom = std::ceil(std::max({ a.y, b.y, c.y }) + reach);
        if (right <= 0.0f || bottom <= 0.0f || left >= static_cast<f32>(mask.width) || top >= static_cast<f32>(mask.height)) return;
        const u32 x0 = static_cast<u32>(std::max(left, 0.0f)), y0 = static_cast<u32>(std::max(top, 0.0f));
        const u32 x1 = static_cast<u32>(std::min(right, static_cast<f32>(mask.width))), y1 = static_cast<u32>(std::min(bottom, static_cast<f32>(mask.height)));

        // Either way round: UVs can be mirrored
        const f32 area = cross(b - a, c - a);
        const f32 side = area < 0.0f ? -1.0f : 1.0f;

        for (u32 y = y0; y < y1; ++y) {
            for (u32 x = x0; x < x1; ++x) {
                u8& pixel = mask.pixels[std::size_t(y) * mask.width + x];
                if (pixel != 0) continue;

                const Vec2 point(static_cast<f32>(x) + 0.5f, static_cast<f32>(y) + 0.5f);
                const bool inside = area != 0.0f && cross(b - a, point - a) * side >= 0.0f && cross(c - b, point - b) * side >= 0.0f && cross(a - c, point - c) * side >= 0.0f;
                const bool near = !inside && reach > 0.0f
                    && std::min({ distanceSquared(point, a, b), distanceSquared(point, b, c), distanceSquared(point, c, a) }) <= reach * reach;
                if (inside || near) pixel = 255;
            }
        }
    }
}

PaintMask maskFromFaces(const MeshData& mesh, const std::vector<FaceHandle>& faces, u32 width, u32 height, f32 reach) {
    PaintMask mask;
    mask.width = width;
    mask.height = height;
    mask.pixels.assign(std::size_t(width) * height, 0);
    const Vec2 size(static_cast<f32>(width), static_cast<f32>(height));

    for (FaceHandle face : faces) {
        // Each triangle corner's UV, found among the face's corners
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        const auto uvOf = [&](VertexHandle vertex) {
            for (std::size_t i = 0; i < vertices.size() && i < uvs.size(); ++i) if (vertices[i] == vertex) return uvs[i];
            return Vec2();
        };

        for (const Triangle& triangle : mesh.getFaceTriangles(face)) {
            const Vec2 a = uvOf(triangle.v0), b = uvOf(triangle.v1), c = uvOf(triangle.v2);

            // Once for every repeat of the texture the triangle touches, moved back onto the picture
            const int firstX = static_cast<int>(std::floor(std::min({ a.x, b.x, c.x }))), lastX = static_cast<int>(std::floor(std::max({ a.x, b.x, c.x })));
            const int firstY = static_cast<int>(std::floor(std::min({ a.y, b.y, c.y }))), lastY = static_cast<int>(std::floor(std::max({ a.y, b.y, c.y })));
            for (int repeatY = firstY; repeatY <= lastY; ++repeatY) {
                for (int repeatX = firstX; repeatX <= lastX; ++repeatX) {
                    const Vec2 shift(static_cast<f32>(repeatX), static_cast<f32>(repeatY));
                    const auto pixels = [&](Vec2 uv) { return Vec2((uv.x - shift.x) * size.x, (uv.y - shift.y) * size.y); };
                    markTriangle(mask, pixels(a), pixels(b), pixels(c), reach);
                }
            }
        }
    }
    return mask;
}
