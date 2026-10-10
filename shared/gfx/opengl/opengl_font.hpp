#pragma once

#include <types>
#include "core/font/bitmap_font.hpp"

class OpenGLFont {
public:
    bool create(const BitmapFont& font);
    void destroy();

    u32 getTexture() const { return m_texture; }

private:
    u32 m_texture = 0;
};
