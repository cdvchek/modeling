#version 330 core

const int MAX_DIRECTIONAL_LIGHTS = 4;

in vec3 v_Normal;

uniform vec3 u_Color;
uniform vec3 u_BackFaceTint;
uniform vec3 u_AmbientColor;
uniform float u_AmbientStrength;

uniform vec3 u_DirectionalLightDirections[MAX_DIRECTIONAL_LIGHTS];
uniform vec3 u_DirectionalLightColors[MAX_DIRECTIONAL_LIGHTS];
uniform int u_DirectionalLightCount;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(v_Normal);
    vec3 color = u_Color;

    // Back faces are lit as if they faced the camera, then tinted so they stand out
    if (!gl_FrontFacing) {
        normal = -normal;
        color *= u_BackFaceTint;
    }

    vec3 light = u_AmbientColor * u_AmbientStrength;

    for (int i = 0; i < u_DirectionalLightCount; ++i) {
        float diffuse = max(dot(normal, -u_DirectionalLightDirections[i]), 0.0);
        light += u_DirectionalLightColors[i] * diffuse;
    }

    FragColor = vec4(color * light, 1.0);
}
