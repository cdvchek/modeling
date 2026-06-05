#pragma once

#include <types>
#include "renderer/shader.hpp"

class OpenGLShader : public Shader {
public:
    ~OpenGLShader();
    bool create(const char* vertex_source, const char* fragment_source);
    bool bind() override;

private:
    u32 m_program = 0;
};