#include "renderer/opengl/shaders/opengl_shader_library.hpp"

namespace {
    // #embed paths are relative to this file; the trailing 0 null-terminates each source
    const unsigned char unlitVert[] = {
        #embed "glsl/unlit.vert"
        , 0
    };
    const unsigned char unlitFrag[] = {
        #embed "glsl/unlit.frag"
        , 0
    };
    const unsigned char litVert[] = {
        #embed "glsl/lit.vert"
        , 0
    };
    const unsigned char litFrag[] = {
        #embed "glsl/lit.frag"
        , 0
    };
    const unsigned char fullscreenVert[] = {
        #embed "glsl/fullscreen.vert"
        , 0
    };
    const unsigned char consoleBackgroundFrag[] = {
        #embed "glsl/console_background.frag"
        , 0
    };
    const unsigned char backgroundFrag[] = {
        #embed "glsl/background.frag"
        , 0
    };
    const unsigned char screenTextVert[] = {
        #embed "glsl/screen_text.vert"
        , 0
    };
    const unsigned char worldTextVert[] = {
        #embed "glsl/world_text.vert"
        , 0
    };
    const unsigned char textFrag[] = {
        #embed "glsl/text.frag"
        , 0
    };
    const unsigned char gridVert[] = {
        #embed "glsl/grid.vert"
        , 0
    };
    const unsigned char gridFrag[] = {
        #embed "glsl/grid.frag"
        , 0
    };

    struct ShaderSource {
        ShaderId id;
        const char* name;
        const unsigned char* vertex;
        const unsigned char* fragment;
    };

    const ShaderSource SOURCES[] = {
        { ShaderId::Unlit,             "unlit",              unlitVert,      unlitFrag },
        { ShaderId::Lit,               "lit",                litVert,        litFrag },
        { ShaderId::ScreenText,        "screen_text",        screenTextVert, textFrag },
        { ShaderId::WorldText,         "world_text",         worldTextVert,  textFrag },
        { ShaderId::ConsoleBackground, "console_background", fullscreenVert, consoleBackgroundFrag },
        { ShaderId::Background,        "background",         fullscreenVert, backgroundFrag },
        { ShaderId::Grid,              "grid",               gridVert,       gridFrag },
    };

    static_assert(std::size(SOURCES) == static_cast<u32>(ShaderId::Count), "every ShaderId needs a source");
}

bool OpenGLShaderLibrary::loadAll() {
    bool allLoaded = true;

    for (const ShaderSource& source : SOURCES) {
        OpenGLShader& shader = m_shaders[static_cast<u32>(source.id)];

        allLoaded &= shader.create(
            source.name,
            reinterpret_cast<const char*>(source.vertex),
            reinterpret_cast<const char*>(source.fragment)
        );
    }

    return allLoaded;
}

void OpenGLShaderLibrary::destroy() {
    for (OpenGLShader& shader : m_shaders) {
        shader.destroy();
    }
}

OpenGLShader& OpenGLShaderLibrary::get(ShaderId id) {
    return m_shaders[static_cast<u32>(id)];
}
