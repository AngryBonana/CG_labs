#version 450

layout(set = 0, binding = 0) uniform ModelUBO {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specularColor;
    vec4 flags;
} ubo;

layout(push_constant) uniform Push {
    mat4 lightViewProj;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

void main() {
    gl_Position = pc.lightViewProj * ubo.model * vec4(inPosition, 1.0);
}
