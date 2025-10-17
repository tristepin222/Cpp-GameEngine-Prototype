#version 450

layout(location = 0) in vec4 vColor;      // incoming color from vertex shader
layout(location = 0) out vec4 outColor;   // framebuffer output

void main() {
    outColor = vColor;
}
