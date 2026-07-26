#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
    mat4 viewProj;
    vec4 camPos;
    float flipX;
    float flipY;
} push;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;   // Declared for layout compatibility
layout(set = 1, binding = 2) uniform sampler2D metallicSampler; // Declared for layout compatibility

layout(location = 0) out vec4 outColor;

void main() {
    vec4 baseColor = texture(texSampler, vUV) * vColor;
    // Do NOT discard — allow the GPU alpha blend stage to handle full transparency.
    // The pipeline has VK_BLEND_FACTOR_SRC_ALPHA / VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
    // so semi-transparent pixels blend correctly against the framebuffer.
    outColor = baseColor;
}
