#version 330 core

uniform vec3 u_TopColor;
uniform vec3 u_BottomColor;
uniform float u_ViewportHeight;

out vec4 FragColor;

// Per-pixel noise in [-0.5, 0.5] to hide 8-bit banding
float dither(vec2 pixel) {
    return fract(sin(dot(pixel, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
}

void main() {
    float t = gl_FragCoord.y / u_ViewportHeight;
    vec3 color = mix(u_BottomColor, u_TopColor, t);

    color += dither(gl_FragCoord.xy) / 255.0;

    FragColor = vec4(color, 1.0);
}
