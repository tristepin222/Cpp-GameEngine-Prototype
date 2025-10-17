#version 450

layout(location = 0) out vec3 worldPos;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 color;
    vec3 camPos;
    float scale;
    float fade;
} push;

void main() {
    vec2 quad[4] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );

    vec2 p = quad[gl_VertexIndex];

    // Expand around camera
    vec3 pos = push.camPos + vec3(p.x, 0.0, p.y) * push.fade;
    worldPos = pos;

    // Transform into clip space
    gl_Position = push.viewProj * vec4(pos, 1.0);
}
