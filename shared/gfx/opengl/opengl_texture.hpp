#pragma once

#include <types>

// RGBA8 textures. A GL context must be current.
namespace OpenGLTexture {
    // A texture from 8-bit RGBA pixels, rows from the top, with mipmaps, trilinear filtering, and clamped edges;
    // 0 if it couldn't be made (no pixels, or larger than GL allows)
    u32 create(const u8* pixels, u32 width, u32 height);
    void destroy(u32 texture);

    // Replaces a block of a texture's pixels (width x height RGBA, rows from the top); its mipmaps are stale until refreshed
    void update(u32 texture, u32 x, u32 y, u32 width, u32 height, const u8* pixels);
    void refreshMipmaps(u32 texture);
}
