#pragma once

#include <types>
#include <string>
#include <unordered_map>
#include "gfx/shader.hpp"
#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"

class OpenGLShader : public Shader {
public:
    OpenGLShader() = default;
    ~OpenGLShader();

    OpenGLShader(const OpenGLShader&) = delete;
    OpenGLShader& operator=(const OpenGLShader&) = delete;

    bool create(const char* name, const char* vertexSource, const char* fragmentSource);
    void destroy();
    bool bind() override;

    void setMat4(const char* name, const f32* matrix);
    void setVec2(const char* name, const Vec2& value);
    void setVec3(const char* name, const Vec3& value);
    void setVec3Array(const char* name, const Vec3* values, u32 count);
    void setFloat(const char* name, f32 value);
    void setFloatArray(const char* name, const f32* values, u32 count);
    void setInt(const char* name, i32 value);
    // Points a uniform block at a buffer binding point
    void bindUniformBlock(const char* name, u32 binding);

private:
    i32 getUniformLocation(const char* name);

    u32 m_program = 0;
    std::string m_name;
    std::unordered_map<std::string, i32> m_uniformLocations;
};
