#version 330 core

// A unit square in the XY plane, from gl_VertexID; the picture's first row is its top edge
uniform mat4 u_MVP;

out vec2 v_UV;

const vec2 CORNERS[6] = vec2[6](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)
);

void main() {
    vec2 corner = CORNERS[gl_VertexID];
    gl_Position = u_MVP * vec4(corner, 0.0, 1.0);
    v_UV = vec2(corner.x + 0.5, 0.5 - corner.y);
}
