#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec3 vWorldPos;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
    mat4 viewProj;
    vec4 camPos;
    float scale; // roughness multiplier
    float fade;  // metallic multiplier
} push;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec4 camPos;      // camPos.xyz, w unused
    vec4 ambientLight; // Renderer fallback sun used as ambient fill
    vec4 lightDir;    // Direction in xyz, type in w
    vec4 lightColor;  // Color in xyz, intensity in w
} cam;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicSampler;

layout(location = 0) out vec4 outColor;

// Dynamic screenspace TBN calculation for normal mapping (NaN-safe fallback)
vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(normalSampler, vUV).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(vWorldPos);
    vec3 Q2  = dFdy(vWorldPos);
    vec2 st1 = dFdx(vUV);
    vec2 st2 = dFdy(vUV);

    vec3 N   = normalize(vNormal);
    
    // Compute tangent and bitangent, fallback to standard coordinates if derivatives are zero/collinear
    vec3 T_dir = Q1 * st2.t - Q2 * st1.t;
    vec3 T;
    if (length(T_dir) > 0.0001) {
        T = normalize(T_dir);
    } else {
        T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
        if (length(T) < 0.0001) {
            T = normalize(cross(N, vec3(1.0, 0.0, 0.0)));
        }
    }

    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
    vec4 baseColor = texture(texSampler, vUV) * vColor;
    if (baseColor.a < 0.01) {
        discard; // Early out for completely transparent pixels
    }

    vec3 N = getNormalFromMap();
    vec3 V = normalize(cam.camPos.xyz - vWorldPos);

    // Dynamic light calculations from CameraUBO (negated to point towards the light source)
    vec3 lightDir = normalize(-cam.lightDir.xyz);
    vec3 lightCol = cam.lightColor.rgb * cam.lightColor.a;
    vec3 H = normalize(lightDir + V);

    // Ambient fill comes from the renderer fallback sun.
    vec3 ambient = (0.05 * cam.ambientLight.rgb + vec3(0.02)) * baseColor.rgb;

    // Diffuse lighting
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = diff * baseColor.rgb * lightCol;

    // Read roughness and metallic from texture and push constants
    vec4 metRough = texture(metallicSampler, vUV);
    float roughness = clamp(push.scale * metRough.g, 0.05, 0.95);
    float metallic  = clamp(push.fade * metRough.r, 0.0, 1.0);

    // Specular shininess mapping (higher roughness = lower shininess)
    float shininess = exp2(10.0 * (1.0 - roughness) + 2.0);

    // Specular highlight (Blinn-Phong)
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specularColor = mix(vec3(0.04), baseColor.rgb, metallic);
    vec3 specular = spec * specularColor * lightCol;

    outColor = vec4(ambient + diffuse + specular, baseColor.a);
}
