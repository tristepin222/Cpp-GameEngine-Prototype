#version 450

layout(location = 0) in vec3 worldPos;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 color;
    vec3 camPos;
    float scale;
    float fade;
} push;

void main() {
    vec2 gridCoord = worldPos.xz / push.scale;
    vec2 f = abs(fract(gridCoord - 0.5) - 0.5);
    float line = min(f.x, f.y);
    float intensity = 1.0 - smoothstep(0.0, 0.02, line);
    float fadeFactor = clamp(1.0 - length(worldPos.xz - push.camPos.xz) / push.fade, 0.0, 1.0);

    outColor = vec4(push.color.rgb, intensity * fadeFactor);
}
