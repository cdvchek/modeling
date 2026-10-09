#version 330 core

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_LOCAL_LIGHTS = 8;
const float PI = 3.14159265;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec2 v_UV;

// The surface (metallic-roughness, as glTF). Colors arrive as sRGB.
uniform vec3 u_BaseColor;
uniform float u_Roughness;
uniform float u_Metallic;
uniform vec3 u_EmissiveColor;       // sRGB
uniform float u_EmissiveStrength;
uniform float u_Opacity;
uniform int u_BackFaces;            // 0 tinted (clay view), 1 lit like front faces (double-sided)
uniform float u_Highlight;          // 0 to 1: how far toward the selection color
uniform vec3 u_HighlightColor;

// Everything about the lights and the view, the same for every object in a frame: one buffer, uploaded when it
// changes. std140 layout: vec4s, with spare w components carrying scalars.
layout(std140) uniform Lighting {
    vec4 u_DirectionalLightDirections[MAX_DIRECTIONAL_LIGHTS];
    vec4 u_DirectionalLightColors[MAX_DIRECTIONAL_LIGHTS];     // linear, intensity included
    vec4 u_LocalLightPositions[MAX_LOCAL_LIGHTS];               // w: range
    vec4 u_LocalLightDirections[MAX_LOCAL_LIGHTS];              // w: cosine of the inner cone
    vec4 u_LocalLightColors[MAX_LOCAL_LIGHTS];                  // w: cosine of the outer cone
    vec4 u_SkyColor;                // linear; w: exposure multiplier (2 to the power of the stops)
    vec4 u_GroundColor;             // linear
    vec4 u_CameraPosition;          // w: 1 shows the UV checker in place of base colors
    vec4 u_BackFaceTint;            // sRGB
    ivec4 u_LightCounts;            // x: directional, y: point and spot
};

out vec4 FragColor;

vec3 srgbToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
}

vec3 linearToSrgb(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), color));
}

// Khronos PBR Neutral: leaves colors alone up to about 0.76, then rolls highlights off toward white instead of clipping
vec3 toneMap(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

struct Surface {
    vec3 normal;
    vec3 view;          // toward the camera
    vec3 diffuse;       // base color with metals' share removed
    vec3 f0;            // reflectance straight on
    float alpha;        // roughness squared
};

vec3 fresnel(vec3 f0, float cosTheta) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

// One light arriving from direction l with the given color; scaled by pi so a plain diffuse surface lights as color x cos
vec3 shade(Surface s, vec3 l, vec3 color) {
    float nl = max(dot(s.normal, l), 0.0);
    if (nl <= 0.0) return vec3(0.0);

    vec3 h = normalize(s.view + l);
    float nv = max(dot(s.normal, s.view), 1e-4);
    float nh = max(dot(s.normal, h), 0.0);
    float a2 = s.alpha * s.alpha;

    // GGX distribution, Smith height-correlated visibility, Schlick Fresnel
    float denom = nh * nh * (a2 - 1.0) + 1.0;
    float distribution = a2 / (PI * denom * denom);
    float visibility = 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
    vec3 f = fresnel(s.f0, max(dot(s.view, h), 0.0));

    vec3 specular = distribution * visibility * f;
    vec3 diffuse = (1.0 - f) * s.diffuse / PI;
    return (diffuse + specular) * color * nl * PI;
}

// The environment in a direction, the sky-ground edge softened by blur (0 sharp, 1 an even mix)
vec3 environment(vec3 direction, float blur) {
    float width = 0.04 + blur * 1.2;
    return mix(u_GroundColor.rgb, u_SkyColor.rgb, smoothstep(-width, width, direction.y));
}

// Analytic stand-in for a prefiltered environment's reflectance (Karis, "Physically Based Shading on Mobile")
vec3 environmentBrdf(vec3 f0, float roughness, float nv) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

// A colored grid for seeing UVs: 8 x 8 cells, hue changing across and brightness down, alternate cells lighter, and a
// thin dark line around each, so stretching, flips, and rotations show (sRGB)
vec3 uvChecker(vec2 uv) {
    vec2 cell = floor(uv * 8.0);
    vec3 hue = clamp(abs(mod(cell.x / 8.0 * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    vec3 color = mix(vec3(0.85), hue, 0.55) * mix(1.0, 0.6, cell.y / 7.0);
    if (mod(cell.x + cell.y, 2.0) > 0.5) color = mix(color, vec3(1.0), 0.35);

    vec2 inCell = fract(uv * 8.0);
    vec2 width = fwidth(uv * 8.0) * 1.2;
    float line = max(1.0 - smoothstep(0.0, width.x, min(inCell.x, 1.0 - inCell.x)), 1.0 - smoothstep(0.0, width.y, min(inCell.y, 1.0 - inCell.y)));
    return mix(color, vec3(0.15), line * 0.8);
}

void main() {
    vec3 normal = normalize(v_Normal);
    vec3 baseColor = srgbToLinear(u_CameraPosition.w > 0.5 ? uvChecker(v_UV) : u_BaseColor);
    baseColor = mix(baseColor, srgbToLinear(u_HighlightColor), u_Highlight);

    // Back faces are lit as if they faced the camera; in clay view they're tinted so they stand out
    if (!gl_FrontFacing) {
        normal = -normal;
        if (u_BackFaces == 0) baseColor *= srgbToLinear(u_BackFaceTint.rgb);
    }

    float roughness = clamp(u_Roughness, 0.03, 1.0);
    float metallic = clamp(u_Metallic, 0.0, 1.0);

    Surface s;
    s.normal = normal;
    s.view = normalize(u_CameraPosition.xyz - v_WorldPosition);
    s.diffuse = baseColor * (1.0 - metallic);
    s.f0 = mix(vec3(0.04), baseColor, metallic);
    s.alpha = roughness * roughness;

    vec3 color = vec3(0.0);

    for (int i = 0; i < u_LightCounts.x; ++i) {
        color += shade(s, -u_DirectionalLightDirections[i].xyz, u_DirectionalLightColors[i].rgb);
    }

    for (int i = 0; i < u_LightCounts.y; ++i) {
        float range = u_LocalLightPositions[i].w;
        vec3 toLight = u_LocalLightPositions[i].xyz - v_WorldPosition;
        float distance = length(toLight);
        if (distance >= range) continue;

        vec3 lightDirection = toLight / max(distance, 1e-4);

        // Smooth falloff from full at the light to zero at its range
        float ratio = distance / range;
        float falloff = 1.0 - ratio * ratio;
        falloff *= falloff;

        float cone = smoothstep(u_LocalLightColors[i].w, u_LocalLightDirections[i].w, dot(-lightDirection, u_LocalLightDirections[i].xyz));

        color += shade(s, lightDirection, u_LocalLightColors[i].rgb * falloff * cone);
    }

    // Light from the environment: diffuse from the hemisphere above the surface, reflections from where they point
    float nv = max(dot(normal, s.view), 1e-4);
    vec3 reflected = reflect(-s.view, normal);
    vec3 environmentSpecular = environment(reflected, roughness) * environmentBrdf(s.f0, roughness, nv);
    vec3 environmentDiffuse = environment(normal, 1.0) * s.diffuse * (1.0 - environmentBrdf(s.f0, roughness, nv));
    color += environmentDiffuse + environmentSpecular;

    color += srgbToLinear(u_EmissiveColor) * u_EmissiveStrength;

    vec3 exposed = color * u_SkyColor.w;
    FragColor = vec4(linearToSrgb(clamp(toneMap(max(exposed, vec3(0.0))), 0.0, 1.0)), u_Opacity);
}
