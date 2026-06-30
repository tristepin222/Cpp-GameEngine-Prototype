#version 450

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
    mat4 viewProj;
    vec3 camPos;
    float scale;
    float fade;
} push;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cam;

layout(location = 0) out vec4 vColor;

void main() {
    gl_Position = cam.viewProj * push.model * vec4(inPos, 1.0);
    vColor = push.color;
}
