#pragma once

#include <array>
#include "renderer/opengl/shaders/opengl_shader.hpp"

enum class ShaderId : u32 {
    Unlit,
    Lit,
    ScreenText,
    WorldText,
    ConsoleBackground,
    Background,
    Grid,
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
