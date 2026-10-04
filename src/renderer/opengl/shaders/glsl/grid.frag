#version 330 core

in vec3 v_Near;
in vec3 v_Far;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraPosition;
uniform float u_FarPlane;
uniform float u_Spacing;
uniform float u_LevelBlend;

out vec4 FragColor;

const float MINOR_ALPHA = 0.18;
const float MAJOR_ALPHA = 0.4;
const float AXIS_ALPHA = 0.9;

const vec3 LINE_COLOR = vec3(0.55);
const vec3 X_AXIS_COLOR = vec3(0.9, 0.25, 0.3);
const vec3 Z_AXIS_COLOR = vec3(0.25, 0.5, 0.95);

float gridLine(vec2 coord, float spacing, float widthPx) {
    vec2 cell = coord / spacing;
    vec2 cellsPerPx = fwidth(cell);
    vec2 distPx = abs(fract(cell - 0.5) - 0.5) / cellsPerPx;

    float coverage = clamp(widthPx * 0.5 + 0.5 - min(distPx.x, distPx.y), 0.0, 1.0);

    // Fade lines out before they get dense enough to moire.
    float density = 1.0 - smoothstep(0.15, 0.4, max(cellsPerPx.x, cellsPerPx.y));

    return coverage * density;
}

float axisLine(float coord, float widthPx) {
    float distPx = abs(coord) / fwidth(coord);
    return clamp(widthPx * 0.5 + 0.5 - distPx, 0.0, 1.0);
}

void main() {
    // Intersect the view ray with the y = 0 plane.
    vec3 ray = v_Far - v_Near;
    float t = -v_Near.y / (abs(ray.y) > 1e-6 ? ray.y : 1e-6);
    vec3 world = v_Near + ray * t;

    vec4 clip = u_ViewProjection * vec4(world, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;

    // Three spacing levels; the finest fades out as the next zoom level approaches.
    float f = u_LevelBlend;
    float minor = gridLine(world.xz, u_Spacing, 1.0) * (1.0 - f) * MINOR_ALPHA;
    float mid = gridLine(world.xz, u_Spacing * 5.0, mix(2.0, 1.0, f)) * mix(MAJOR_ALPHA, MINOR_ALPHA, f);
    float major = gridLine(world.xz, u_Spacing * 25.0, 2.0) * f * MAJOR_ALPHA;

    float alpha = max(minor, max(mid, major));
    vec3 color = LINE_COLOR;

    float xAxis = axisLine(world.z, 2.0);
    float zAxis = axisLine(world.x, 2.0);

    color = mix(color, X_AXIS_COLOR, xAxis);
    color = mix(color, Z_AXIS_COLOR, zAxis);
    alpha = max(alpha, max(xAxis, zAxis) * AXIS_ALPHA);

    float distance = length(world - u_CameraPosition);
    alpha *= 1.0 - smoothstep(u_FarPlane * 0.5, u_FarPlane, distance);

    if (t <= 0.0 || t >= 1.0 || alpha < 0.005)
        discard;

    FragColor = vec4(color, alpha);
}
