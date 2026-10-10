#version 330 core

uniform mat4 u_InverseViewProjection;

out vec3 v_Near;
out vec3 v_Far;

vec3 unproject(vec2 ndc, float depth) {
    vec4 world = u_InverseViewProjection * vec4(ndc, depth, 1.0);
    return world.xyz / world.w;
}

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 ndc = positions[gl_VertexID];

    v_Near = unproject(ndc, -1.0);
    v_Far = unproject(ndc, 1.0);

    gl_Position = vec4(ndc, 0.0, 1.0);
}
