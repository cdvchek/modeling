#version 330 core

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_LOCAL_LIGHTS = 8;

in vec3 v_WorldPosition;
in vec3 v_Normal;

uniform vec3 u_Color;
uniform vec3 u_BackFaceTint;
uniform vec3 u_AmbientColor;
uniform float u_AmbientStrength;

uniform vec3 u_DirectionalLightDirections[MAX_DIRECTIONAL_LIGHTS];
uniform vec3 u_DirectionalLightColors[MAX_DIRECTIONAL_LIGHTS];
uniform int u_DirectionalLightCount;

// Point and spot lights; a point light is a spot whose cone covers every direction
uniform vec3 u_LocalLightPositions[MAX_LOCAL_LIGHTS];
uniform vec3 u_LocalLightDirections[MAX_LOCAL_LIGHTS];
uniform vec3 u_LocalLightColors[MAX_LOCAL_LIGHTS];
uniform float u_LocalLightRanges[MAX_LOCAL_LIGHTS];
uniform float u_LocalLightCosInner[MAX_LOCAL_LIGHTS];
uniform float u_LocalLightCosOuter[MAX_LOCAL_LIGHTS];
uniform int u_LocalLightCount;

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

    for (int i = 0; i < u_LocalLightCount; ++i) {
        vec3 toLight = u_LocalLightPositions[i] - v_WorldPosition;
        float distance = length(toLight);
        if (distance >= u_LocalLightRanges[i]) continue;

        vec3 lightDirection = toLight / max(distance, 1e-4);
        float diffuse = max(dot(normal, lightDirection), 0.0);

        // Smooth falloff from full at the light to zero at its range
        float ratio = distance / u_LocalLightRanges[i];
        float falloff = 1.0 - ratio * ratio;
        falloff *= falloff;

        float cone = smoothstep(u_LocalLightCosOuter[i], u_LocalLightCosInner[i], dot(-lightDirection, u_LocalLightDirections[i]));

        light += u_LocalLightColors[i] * diffuse * falloff * cone;
    }

    FragColor = vec4(color * light, 1.0);
}
