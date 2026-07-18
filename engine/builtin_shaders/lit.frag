#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec3 vWorldPos;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
    mat4 viewProj;
    vec3 camPos;
    float scale; // roughness multiplier
    float fade;  // metallic multiplier
} push;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicSampler;

layout(location = 0) out vec4 outColor;

// Dynamic screenspace TBN calculation for normal mapping
vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(normalSampler, vUV).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(vWorldPos);
    vec3 Q2  = dFdy(vWorldPos);
    vec2 st1 = dFdx(vUV);
    vec2 st2 = dFdy(vUV);

    vec3 N   = normalize(vNormal);
    vec3 T   = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B   = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
    vec4 baseColor = texture(texSampler, vUV) * vColor;
    if (baseColor.a < 0.01) {
        discard; // Early out for completely transparent pixels
    }

    vec3 N = getNormalFromMap();
    vec3 V = normalize(push.camPos - vWorldPos);

    // Hardcoded directional sun light
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 H = normalize(lightDir + V);

    // Ambient lighting
    vec3 ambient = 0.2 * baseColor.rgb;

    // Diffuse lighting
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = diff * baseColor.rgb;

    // Read roughness and metallic from texture and push constants
    vec4 metRough = texture(metallicSampler, vUV);
    float roughness = clamp(push.scale * metRough.g, 0.05, 0.95);
    float metallic  = clamp(push.fade * metRough.r, 0.0, 1.0);

    // Specular shininess mapping (higher roughness = lower shininess)
    float shininess = exp2(10.0 * (1.0 - roughness) + 2.0);

    // Specular highlight (Blinn-Phong)
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specularColor = mix(vec3(0.04), baseColor.rgb, metallic);
    vec3 specular = spec * specularColor;

    outColor = vec4(ambient + diffuse + specular, baseColor.a);
}
