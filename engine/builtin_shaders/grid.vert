#version 450

layout(location = 0) out vec3 worldPos;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 color;
    vec4 camPos;
    float scale;
    float fade;
} push;

void main() {
    vec2 quad[6] = vec2[](
        vec2(-1.0, -1.0), // bottom-left
        vec2( 1.0, -1.0), // bottom-right
        vec2( 1.0,  1.0), // top-right
        vec2(-1.0, -1.0), // bottom-left
        vec2( 1.0,  1.0), // top-right
        vec2(-1.0,  1.0)  // top-left
    );

    vec2 p = quad[gl_VertexIndex];

    // Expand around camera in XZ plane at Y=0
    vec3 pos = vec3(push.camPos.x + p.x * push.fade, 0.0, push.camPos.z + p.y * push.fade);
    worldPos = pos;

    // Transform into clip space
    gl_Position = push.viewProj * vec4(pos, 1.0);
}
