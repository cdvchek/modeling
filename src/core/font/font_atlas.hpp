#pragma once

#include <types>
#include "core/font/bitmap_font.hpp"

struct GlyphUV {
    f32 u0;
    f32 v0;
    f32 u1;
    f32 v1;
};

// Glyphs are laid out in a 16 x 6 grid in character order; UVs don't depend on glyph size
namespace FontAtlas {
    constexpr u32 COLUMNS = 16;
    constexpr u32 ROWS = 6;

    inline GlyphUV glyphUV(char character) {
        u32 code = static_cast<unsigned char>(character);
        if (code < BitmapFont::FIRST_CHAR || code > BitmapFont::LAST_CHAR) code = ' ';

        const u32 index = code - BitmapFont::FIRST_CHAR;
        const f32 cellWidth = 1.0f / COLUMNS;
        const f32 cellHeight = 1.0f / ROWS;

        const f32 u0 = (index % COLUMNS) * cellWidth;
        const f32 v0 = (index / COLUMNS) * cellHeight;

        return { u0, v0, u0 + cellWidth, v0 + cellHeight };
    }
}
