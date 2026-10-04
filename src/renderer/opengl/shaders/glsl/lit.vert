#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

uniform mat4 u_MVP;
uniform mat4 u_NormalMatrix;

out vec3 v_Normal;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    v_Normal = mat3(u_NormalMatrix) * a_Normal;
}
