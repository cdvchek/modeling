#pragma once

#include <vector>
#include <types>
#include "scene/mesh/mesh_data.hpp"

// Where on a texture paint may land: how much of a dab each pixel takes, 0 for none to 255 for all of it
struct PaintMask {
    u32 width = 0;      // the texture's size
    u32 height = 0;
    std::vector<u8> pixels;     // rows from the top

    u8 at(u32 x, u32 y) const { return pixels[std::size_t(y) * width + x]; }
};

// How far past a face's edge its mask reaches by default, in pixels, so filtering along a seam never shows a thin unpainted line
inline constexpr f32 MASK_REACH = 1.0f;

// The texture pixels the faces' UVs cover: every pixel whose middle is in one of their triangles or within reach pixels of it
PaintMask maskFromFaces(const MeshData& mesh, const std::vector<FaceHandle>& faces, u32 width, u32 height, f32 reach = MASK_REACH);
