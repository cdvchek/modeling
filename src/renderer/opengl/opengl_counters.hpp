#pragma once

#include <glad/glad.h>
#include "renderer/render_stats.hpp"

// Every draw call in the OpenGL backend goes through these, so the stats readout counts them all.
// One renderer and one GL context, so the counters are one global set, reset each frame.

inline RenderStats& glCounters() {
    static RenderStats counters;
    return counters;
}

inline void countPrimitives(GLenum mode, GLsizei count) {
    RenderStats& counters = glCounters();
    ++counters.drawCalls;
    if (mode == GL_TRIANGLES) counters.triangles += static_cast<u64>(count) / 3;
    else if (mode == GL_LINES) counters.lines += static_cast<u64>(count) / 2;
    else if (mode == GL_POINTS) counters.points += static_cast<u64>(count);
}

inline void drawElements(GLenum mode, GLsizei count, GLenum type, const void* offset) {
    countPrimitives(mode, count);
    glDrawElements(mode, count, type, offset);
}

inline void drawArrays(GLenum mode, GLint first, GLsizei count) {
    countPrimitives(mode, count);
    glDrawArrays(mode, first, count);
}
