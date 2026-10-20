#version 450

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 ambient;
    vec4 dirDirection;
    vec4 dirColor;
    vec4 lightCounts;
} scene;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specularColor;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vColor;

void main() {
    vec4 world = ubo.model * vec4(inPosition, 1.0);
    vWorldPos = world.xyz;
    vNormal = normalize(mat3(ubo.normalMatrix) * inNormal);
    vColor = inColor * ubo.albedo.rgb;
    gl_Position = scene.proj * scene.view * world;
}