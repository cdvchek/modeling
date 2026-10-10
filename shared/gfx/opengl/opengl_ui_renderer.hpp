#pragma once

#include <array>
#include <types>
#include "ui/ui_draw_list.hpp"
#include "gfx/opengl/opengl_font.hpp"
#include "gfx/opengl/opengl_shader.hpp"

// Draws a UIDrawList with its own shader (glsl/ui.vert, glsl/ui.frag)
class OpenGLUIRenderer {
public:
    // False if the UI shader failed to load; draw then does nothing
    bool create();
    void destroy();

    // Uploads the whole list once, then draws each batch with its clip rect and font texture
    void draw(const UIDrawList& list, const std::array<OpenGLFont, static_cast<u32>(FontId::Count)>& fonts, u32 viewportWidth, u32 viewportHeight);

private:
    OpenGLShader m_shader;
    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ebo = 0;
};
