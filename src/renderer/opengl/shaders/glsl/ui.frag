#version 330 core

in vec2 v_UV;
in vec2 v_Local;
in vec2 v_HalfSize;  // ring slices: inner and outer radius
in vec4 v_Shape;     // radius, border width, blur, mode; ring slices: half-angle, border width, gap, mode
in vec4 v_Fill;
in vec4 v_Border;

uniform sampler2D u_Texture;

out vec4 FragColor;

// Signed distance to a rounded rectangle centered on the origin; negative inside
float roundedRectDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 q = abs(point) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// Signed distance to a ring slice pointing along +x, with half the gap trimmed from each side
float ringSliceDistance(vec2 point, float inner, float outer, float halfAngle, float gap) {
    float r = length(point);
    float radial = max(inner - r, r - outer);
    float side = dot(vec2(point.x, abs(point.y)), vec2(-sin(halfAngle), cos(halfAngle))) + gap * 0.5;
    return max(radial, side);
}

vec4 shade(float distance, float borderWidth) {
    // One pixel of anti-aliasing at the outer edge and at the border's inner edge
    float coverage = clamp(0.5 - distance, 0.0, 1.0);
    vec4 color = v_Fill;

    if (borderWidth > 0.0) {
        float inside = clamp(0.5 - (distance + borderWidth), 0.0, 1.0);
        color = mix(v_Border, v_Fill, inside);
    }

    return vec4(color.rgb, color.a * coverage);
}

void main() {
    float radius = v_Shape.x;
    float borderWidth = v_Shape.y;
    float blur = v_Shape.z;
    float mode = v_Shape.w;

    if (mode > 1.5) {
        FragColor = shade(ringSliceDistance(v_Local, v_HalfSize.x, v_HalfSize.y, radius, blur), borderWidth);
    } else if (mode > 0.5) {
        FragColor = vec4(v_Fill.rgb, v_Fill.a * texture(u_Texture, v_UV).r);
    } else {
        float distance = roundedRectDistance(v_Local, v_HalfSize, min(radius, min(v_HalfSize.x, v_HalfSize.y)));

        if (blur > 0.0) {
            FragColor = vec4(v_Fill.rgb, v_Fill.a * (1.0 - smoothstep(-blur, blur, distance)));
        } else {
            FragColor = shade(distance, borderWidth);
        }
    }

    if (FragColor.a <= 0.0) discard;
}
