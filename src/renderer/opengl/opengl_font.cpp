#include "renderer/opengl/opengl_font.hpp"

#include <vector>


bool OpenGLFont::create(const BitmapFont& font) {

    constexpr u32 COLUMNS = 16;
    constexpr u32 ROWS = 6;

    constexpr u32 ATLAS_WIDTH =
        COLUMNS * BitmapGlyph::WIDTH;

    constexpr u32 ATLAS_HEIGHT =
        ROWS * BitmapGlyph::HEIGHT;


    std::vector<u8> atlas(
        ATLAS_WIDTH * ATLAS_HEIGHT,
        0
    );


    // --------------------------------
    // Copy glyphs into atlas
    // --------------------------------

    for (u32 code = BitmapFont::FIRST_CHAR;
         code <= BitmapFont::LAST_CHAR;
         ++code) {

        const BitmapGlyph* glyph =
            font.getGlyph(static_cast<char>(code));

        if (!glyph) {
            continue;
        }


        u32 index =
            code - BitmapFont::FIRST_CHAR;

        u32 column = index % COLUMNS;
        u32 row = index / COLUMNS;


        u32 atlasX =
            column * BitmapGlyph::WIDTH;

        u32 atlasY =
            row * BitmapGlyph::HEIGHT;


        for (u32 y = 0;
             y < BitmapGlyph::HEIGHT;
             ++y) {

            for (u32 x = 0;
                 x < BitmapGlyph::WIDTH;
                 ++x) {

                u32 sourceIndex =
                    y * BitmapGlyph::WIDTH + x;

                u32 destinationIndex =
                    (atlasY + y) * ATLAS_WIDTH +
                    (atlasX + x);

                atlas[destinationIndex] =
                    glyph->pixels[sourceIndex];
            }
        }
    }


    // --------------------------------
    // Create OpenGL texture
    // --------------------------------

    glGenTextures(1, &m_texture);

    glBindTexture(
        GL_TEXTURE_2D,
        m_texture
    );


    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R8,
        ATLAS_WIDTH,
        ATLAS_HEIGHT,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        atlas.data()
    );


    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );


    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );


    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void OpenGLFont::destroy() {

    if (m_texture != 0) {

        glDeleteTextures(
            1,
            &m_texture
        );

        m_texture = 0;
    }
}

GlyphUV OpenGLFont::getGlyphUV(char character) const {

    u32 index =
        static_cast<u32>(character) -
        BitmapFont::FIRST_CHAR;

    u32 column = index % 16;
    u32 row = index / 16;

    float cellWidth = 1.0f / 16.0f;
    float cellHeight = 1.0f / 6.0f;

    float u0 = column * cellWidth;
    float u1 = u0 + cellWidth;

    float v0 = row * cellHeight;
    float v1 = v0 + cellHeight;

    return {
        u0,
        v0,
        u1,
        v1
    };
}