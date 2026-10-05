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
