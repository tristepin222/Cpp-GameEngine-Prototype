#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in ivec4 inBoneIDs;
layout(location = 4) in vec4 inBoneWeights;

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

layout(set = 2, binding = 0) uniform JointPalette {
    mat4 joints[128]; // Max 128 bones supported
} palette;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;

void main() {
    // Linear Blend Skinning formula:
    // Accumulate weighted bone matrices for this vertex
    mat4 skinMat = 
        palette.joints[inBoneIDs.x] * inBoneWeights.x +
        palette.joints[inBoneIDs.y] * inBoneWeights.y +
        palette.joints[inBoneIDs.z] * inBoneWeights.z +
        palette.joints[inBoneIDs.w] * inBoneWeights.w;

    vec4 skinnedPos = skinMat * vec4(inPos, 1.0);
    gl_Position = cam.viewProj * push.model * skinnedPos;
    
    vColor = push.color;
    vUV = inUV;
}
