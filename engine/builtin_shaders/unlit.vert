#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
    mat4 viewProj;
    vec4 camPos;
    float scale;
    float fade;
} push;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec4 camPos;      // camPos.xyz, w unused
    vec4 lightDir;    // Direction in xyz, type in w
    vec4 lightColor;  // Color in xyz, intensity in w
} cam;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;

void main() {
    gl_Position = cam.viewProj * push.model * vec4(inPos, 1.0);
    vColor = push.color;
    vUV = inUV;
}
