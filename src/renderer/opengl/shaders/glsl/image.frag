#version 330 core

in vec2 v_UV;

uniform sampler2D u_Texture;
uniform float u_Opacity;

out vec4 FragColor;

void main() {
    vec4 color = texture(u_Texture, v_UV);
    color.a *= u_Opacity;

    // Fully clear pixels leave the depth buffer alone, so what's behind them still shows
    if (color.a < 0.004)
        discard;

    FragColor = color;
}
