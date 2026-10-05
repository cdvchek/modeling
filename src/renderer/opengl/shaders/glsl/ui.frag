#version 330 core

in vec2 v_UV;
in vec2 v_Local;
in vec2 v_HalfSize;
in vec4 v_Shape;    // radius, border width, blur, mode
in vec4 v_Fill;
in vec4 v_Border;

uniform sampler2D u_Texture;

out vec4 FragColor;

// Signed distance to a rounded rectangle centered on the origin; negative inside
float roundedRectDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 q = abs(point) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    float radius = v_Shape.x;
    float borderWidth = v_Shape.y;
    float blur = v_Shape.z;
    bool glyph = v_Shape.w > 0.5;

    if (glyph) {
        FragColor = vec4(v_Fill.rgb, v_Fill.a * texture(u_Texture, v_UV).r);
    } else {
        float distance = roundedRectDistance(v_Local, v_HalfSize, min(radius, min(v_HalfSize.x, v_HalfSize.y)));

        if (blur > 0.0) {
            FragColor = vec4(v_Fill.rgb, v_Fill.a * (1.0 - smoothstep(-blur, blur, distance)));
        } else {
            // One pixel of anti-aliasing at the outer edge and at the border's inner edge
            float coverage = clamp(0.5 - distance, 0.0, 1.0);
            vec4 color = v_Fill;

            if (borderWidth > 0.0) {
                float inside = clamp(0.5 - (distance + borderWidth), 0.0, 1.0);
                color = mix(v_Border, v_Fill, inside);
            }

            FragColor = vec4(color.rgb, color.a * coverage);
        }
    }

    if (FragColor.a <= 0.0) discard;
}
