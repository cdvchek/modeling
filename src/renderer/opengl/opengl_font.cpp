#include "renderer/opengl/opengl_font.hpp"
#include "core/font/font_atlas.hpp"

#include <vector>
#include <glad/glad.h>

bool OpenGLFont::create(const BitmapFont& font) {
    destroy();

    const u32 glyphWidth = font.getGlyphWidth();
    const u32 glyphHeight = font.getGlyphHeight();
    if (glyphWidth == 0 || glyphHeight == 0) return false;

    const u32 atlasWidth = FontAtlas::COLUMNS * glyphWidth;
    const u32 atlasHeight = FontAtlas::ROWS * glyphHeight;

    std::vector<u8> atlas(atlasWidth * atlasHeight, 0);

    // 1. Copy glyphs into their grid cells
    for (u32 code = BitmapFont::FIRST_CHAR; code <= BitmapFont::LAST_CHAR; ++code) {
        const BitmapGlyph* glyph = font.getGlyph(static_cast<char>(code));
        if (!glyph) continue;

        const u32 index = code - BitmapFont::FIRST_CHAR;
        const u32 atlasX = (index % FontAtlas::COLUMNS) * glyphWidth;
        const u32 atlasY = (index / FontAtlas::COLUMNS) * glyphHeight;

        for (u32 y = 0; y < glyphHeight; ++y) {
            for (u32 x = 0; x < glyphWidth; ++x) {
                atlas[(atlasY + y) * atlasWidth + atlasX + x] = glyph->pixels[y * glyphWidth + x];
            }
        }
    }

    // 2. Upload as a single-channel texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(atlasWidth), static_cast<GLsizei>(atlasHeight), 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void OpenGLFont::destroy() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}
