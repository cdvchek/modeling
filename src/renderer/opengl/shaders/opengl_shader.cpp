#include "renderer/opengl/shaders/opengl_shader.hpp"

#include <iostream>
#include <glad/glad.h>

namespace {
    GLuint compileStage(const char* shaderName, GLenum stage, const char* source) {
        GLuint shader = glCreateShader(stage);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);

            const char* stageName = stage == GL_VERTEX_SHADER ? "vertex" : "fragment";
            std::cerr << "[shader " << shaderName << "] " << stageName << " compile failed:\n" << infoLog << std::endl;

            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }
}

OpenGLShader::~OpenGLShader() {
    destroy();
}

bool OpenGLShader::create(const char* name, const char* vertexSource, const char* fragmentSource) {
    destroy();
    m_name = name;

    GLuint vertexShader = compileStage(name, GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) return false;

    GLuint fragmentShader = compileStage(name, GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);

    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_program, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "[shader " << name << "] link failed:\n" << infoLog << std::endl;

        destroy();
        return false;
    }

    return true;
}

void OpenGLShader::destroy() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }

    m_uniformLocations.clear();
}

bool OpenGLShader::bind() {
    if (m_program == 0) return false;

    glUseProgram(m_program);
    return true;
}

i32 OpenGLShader::getUniformLocation(const char* name) {
    auto it = m_uniformLocations.find(name);
    if (it != m_uniformLocations.end()) return it->second;

    const i32 location = glGetUniformLocation(m_program, name);
    m_uniformLocations.emplace(name, location);
    return location;
}

void OpenGLShader::setMat4(const char* name, const f32* matrix) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, matrix);
}

void OpenGLShader::setVec2(const char* name, const Vec2& value) {
    glUniform2f(getUniformLocation(name), value.x, value.y);
}

void OpenGLShader::setVec3(const char* name, const Vec3& value) {
    glUniform3f(getUniformLocation(name), value.x, value.y, value.z);
}

void OpenGLShader::setVec3Array(const char* name, const Vec3* values, u32 count) {
    static_assert(sizeof(Vec3) == 3 * sizeof(f32), "Vec3 must be tightly packed");
    glUniform3fv(getUniformLocation(name), static_cast<GLsizei>(count), &values[0].x);
}

void OpenGLShader::setFloat(const char* name, f32 value) {
    glUniform1f(getUniformLocation(name), value);
}

void OpenGLShader::setFloatArray(const char* name, const f32* values, u32 count) {
    glUniform1fv(getUniformLocation(name), static_cast<GLsizei>(count), values);
}

void OpenGLShader::setInt(const char* name, i32 value) {
    glUniform1i(getUniformLocation(name), value);
}
