#pragma once

#include <array>
#include "gfx/opengl/opengl_shader.hpp"

enum class ShaderId : u32 {
    Unlit,
    Lit,
    WorldText,
    Background,
    Grid,
    Image,
    Count
};

class OpenGLShaderLibrary {
public:
    bool loadAll();
    void destroy();

    OpenGLShader& get(ShaderId id);

private:
    std::array<OpenGLShader, static_cast<u32>(ShaderId::Count)> m_shaders;
};
