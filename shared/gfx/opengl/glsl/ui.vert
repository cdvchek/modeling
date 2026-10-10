#version 330 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec2 a_Local;
layout(location = 3) in vec2 a_HalfSize;
layout(location = 4) in vec4 a_Shape;
layout(location = 5) in vec4 a_Fill;
layout(location = 6) in vec4 a_Border;

uniform vec2 u_ViewportSize;

out vec2 v_UV;
out vec2 v_Local;
out vec2 v_HalfSize;
out vec4 v_Shape;
out vec4 v_Fill;
out vec4 v_Border;

void main() {
    // Pixels with y down to clip space with y up
    vec2 ndc = vec2(a_Position.x / u_ViewportSize.x * 2.0 - 1.0, 1.0 - a_Position.y / u_ViewportSize.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);

    v_UV = a_UV;
    v_Local = a_Local;
    v_HalfSize = a_HalfSize;
    v_Shape = a_Shape;
    v_Fill = a_Fill;
    v_Border = a_Border;
}
