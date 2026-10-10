#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_MVP;
uniform float u_DepthBias;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);

    // Pulls geometry slightly toward the camera so points win against the edges meeting at them
    gl_Position.z -= u_DepthBias * gl_Position.w;
}
