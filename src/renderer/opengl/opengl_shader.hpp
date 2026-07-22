#pragma once

#include <types>
#include "renderer/shader.hpp"
#include "core/math/vec3.hpp"

class OpenGLShader : public Shader {
public:
    ~OpenGLShader();
    bool create(const char* vertex_source, const char* fragment_source);
    bool bind() override;

    void setMat4(const char* name, const f32* matrix);
    void setVec3(const char* name, const Vec3& value);
    void setUInt(const char* name, const u32 value);

private:
    u32 m_program = 0;
};