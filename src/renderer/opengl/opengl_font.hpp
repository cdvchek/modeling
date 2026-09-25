#pragma once

#include <glad/glad.h>

#include "core/font/bitmap_font.hpp"

struct GlyphUV {
    float u0;
    float v0;
    float u1;
    float v1;
};

class OpenGLFont {
public:

    bool create(const BitmapFont& font);
    void destroy();

    GLuint getTexture() const {
        return m_texture;
    }

    GlyphUV getGlyphUV(char character) const;

private:

    GLuint m_texture = 0;
};