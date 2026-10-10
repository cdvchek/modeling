#version 330 core

const int POINT_SQUARE = 0;
const int POINT_DISC = 1;
const int POINT_GLOW = 2;

uniform vec3 u_Color;
uniform float u_Alpha;
uniform int u_PointShape;

out vec4 FragColor;

void main() {
    float alpha = u_Alpha;

    // Points only: 0 at the center, 1 at the edge of the point sprite
    if (u_PointShape != POINT_SQUARE) {
        float distance = length(gl_PointCoord - vec2(0.5)) * 2.0;

        if (u_PointShape == POINT_DISC) {
            float edge = fwidth(distance);
            alpha *= 1.0 - smoothstep(1.0 - edge, 1.0, distance);
        } else {
            float falloff = clamp(1.0 - distance, 0.0, 1.0);
            alpha *= falloff * falloff;
        }
    }

    if (alpha <= 0.0) discard;

    FragColor = vec4(u_Color, alpha);
}
