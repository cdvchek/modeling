#pragma once

#include <array>
#include <types>
#include "ui/ui_draw_list.hpp"
#include "renderer/opengl/opengl_font.hpp"
#include "renderer/opengl/shaders/opengl_shader.hpp"

class OpenGLUIRenderer {
public:
    void create();
    void destroy();

    // Uploads the whole list once, then draws each batch with its clip rect and font texture
    void draw(const UIDrawList& list, OpenGLShader& shader, const std::array<OpenGLFont, static_cast<u32>(FontId::Count)>& fonts, u32 viewportWidth, u32 viewportHeight);

private:
    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ebo = 0;
};
