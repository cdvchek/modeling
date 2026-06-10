#include "renderer/opengl/opengl_shader.hpp"

#include <glad/glad.h>

OpenGLShader::~OpenGLShader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

bool OpenGLShader::create(const char* vertex_source, const char* fragment_source) {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_source, nullptr);
    glCompileShader(vertex_shader);

    GLint success = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);

        // std::cout << "Vertex shader compilation failed:\n"
        //           << info_log << '\n';

        glDeleteShader(vertex_shader);
        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_source, nullptr);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);

        // std::cout << "Fragment shader compilation failed:\n"
        //           << info_log << '\n';

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        return false;
    }

    m_program = glCreateProgram();

    glAttachShader(m_program, vertex_shader);
    glAttachShader(m_program, fragment_shader);

    glLinkProgram(m_program);

    glGetProgramiv(m_program, GL_LINK_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(m_program, 512, nullptr, info_log);

        // std::cout << "Shader program linking failed:\n"
        //           << info_log << '\n';

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(m_program);

        m_program = 0;

        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return true;
}

bool OpenGLShader::bind() {
    if (m_program == 0) {
        return false;
    }

    glUseProgram(m_program);

    return true;
}

void OpenGLShader::setMat4(const char* name, const f32* matrix) {
    GLint location = glGetUniformLocation(m_program, name);
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
}

void OpenGLShader::setVec3(const char* name, const Vec3& value) {
    GLint location = glGetUniformLocation(m_program, name);
    glUniform3f(location, value.x, value.y, value.z);
}