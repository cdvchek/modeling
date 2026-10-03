#include "renderer/opengl/opengl_renderer.hpp"
#include "core/font/embedded_fonts.hpp"

#include <stdexcept>
#include <windows.h>
#include <glad/glad.h>

const char* vertex_source = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_MVP;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
)";

const char* fragment_source = R"(
#version 330 core

uniform vec3 u_Color;

out vec4 FragColor;

void main() {
    FragColor = vec4(u_Color, 1.0);
}
)";

const char* console_vert_source = R"(
#version 330 core

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";

const char* console_frag_source = R"(
#version 330 core

out vec4 FragColor;

void main() {
    FragColor = vec4(0.15, 0.15, 0.15, 0.7);
}
)";

const char* console_text_vert_source = R"(
#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPosition, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* console_text_frag_source = R"(
#version 330 core

in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uFont;
uniform vec3 uColor;

void main()
{
    float alpha = texture(uFont, TexCoord).r;

    FragColor = vec4(uColor, alpha);
}
)";

const char* text_3d_vert_source = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;

uniform mat4 u_MVP;

out vec2 v_UV;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    v_UV = a_UV;
}
)";

const char* text_3d_frag_source = R"(
#version 330 core

in vec2 v_UV;

uniform sampler2D u_Texture;
uniform vec3 u_Color;

out vec4 FragColor;

void main() {
    float alpha = texture(u_Texture, v_UV).r;

    if (alpha < 0.01)
        discard;

    FragColor = vec4(u_Color, alpha);
}
)";

bool OpenGLRenderer::initialize(void* window, void* surface, const RendererConfig& config) {
    if (m_initialized) return true;
    if (window == nullptr) return false;
    if (surface == nullptr) return false;

    m_width = config.width;
    m_height = config.height;

    m_window = window;
    m_surface = surface;

    m_vsyncEnabled = config.enableVSync;

    HDC w32Surface = static_cast<HDC>(surface);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    
    int pixelFormat = ChoosePixelFormat(w32Surface, &pfd);
    if (pixelFormat == 0) {
        return false;
    }
    
    if (!SetPixelFormat(w32Surface, pixelFormat, &pfd)) {
        return false;
    }
    
    HGLRC w32glContext = wglCreateContext(w32Surface);

    if (!w32glContext) {
        return false;
    }

    m_glContext = static_cast<void*>(w32glContext);
    
    if (!wglMakeCurrent(w32Surface, w32glContext)) {
        wglDeleteContext(w32glContext);
        m_glContext = nullptr;
        return false;
    }
    
    if (!gladLoadGL()) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(w32glContext);
        m_glContext = nullptr;
        return false;
    }
    
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!version) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(w32glContext);
        m_glContext = nullptr;
        return false;
    }

    glViewport(0, 0, static_cast<GLsizei>(config.width), static_cast<GLsizei>(config.height));
    glEnable(GL_DEPTH_TEST);

    glGenVertexArrays(1, &m_textVAO);
    glGenBuffers(1, &m_textVBO);

    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float) * 6 * 4,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        (void*)(2 * sizeof(float))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_debugLineVAO);
    glGenBuffers(1, &m_debugLineVBO);

    glBindVertexArray(m_debugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugLineVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float) * 6,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glGenVertexArrays(1, &m_text3DVAO);
    glGenBuffers(1, &m_text3DVBO);

    glBindVertexArray(m_text3DVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_text3DVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(f32) * 6 * 5,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    // Position: x, y, z
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(f32),
        (void*)0
    );

    // UV: u, v
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(f32),
        (void*)(3 * sizeof(f32))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    setVSync(config.enableVSync);

    m_testShader = std::make_unique<OpenGLShader>();
    m_testShader->create(vertex_source, fragment_source);

    m_consoleShader = std::make_unique<OpenGLShader>();
    m_consoleShader->create(console_vert_source, console_frag_source);

    m_textShader = std::make_unique<OpenGLShader>();
    m_textShader->create(console_text_vert_source, console_text_frag_source);

    m_text3DShader = std::make_unique<OpenGLShader>();
    m_text3DShader->create(text_3d_vert_source, text_3d_frag_source);

    BitmapFont font;
    if (!font.loadFromMemory(EmbeddedFonts::console, EmbeddedFonts::consoleSize)) return false;
    m_consoleFont.create(font);

    m_initialized = true;
    return true;
}

void OpenGLRenderer::shutdown() {
    if (!m_initialized) return;

    HGLRC w32glContext = static_cast<HGLRC>(m_glContext);

    if (w32glContext) {
        if (wglGetCurrentContext() == w32glContext) {
            wglMakeCurrent(nullptr, nullptr);
        }

        wglDeleteContext(w32glContext);
    }

    m_glContext = nullptr;
    m_window = nullptr;
    m_surface = nullptr;
    m_initialized = false;
}

void OpenGLRenderer::setVSync(bool enabled) {
    m_vsyncEnabled = enabled;

    using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int);

    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
            wglGetProcAddress("wglSwapIntervalEXT")
        );

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(enabled ? 1 : 0);
    }
}

void OpenGLRenderer::present() {
    if (!m_initialized) return;
    if (m_surface == nullptr) return;

    HDC w32surface = static_cast<HDC>(m_surface);
    SwapBuffers(w32surface);
}