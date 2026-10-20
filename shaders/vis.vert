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

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 vColor;

void main() {
    vColor = pc.color.rgb;
    gl_Position = scene.proj * scene.view * pc.model * vec4(inPosition, 1.0);
}