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

const int MAX_LIGHTS = 64;

struct PointLight {
    vec4 positionAndIntensity;
    vec4 color;
    vec4 params;
};

struct SpotLight {
    vec4 positionAndIntensity;
    vec4 direction;
    vec4 color;
    vec4 angles;
    vec4 params;
};

layout(set = 0, binding = 1) readonly buffer PointLightSSBO {
    PointLight lights[MAX_LIGHTS];
} pointLights;

layout(set = 0, binding = 2) readonly buffer SpotLightSSBO {
    SpotLight lights[MAX_LIGHTS];
} spotLights;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specularColor;
} ubo;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;

layout(location = 0) out vec4 outColor;

vec3 blinnPhong(vec3 N, vec3 V, vec3 L, vec3 lightColor, float intensity,
                float attenuation, vec3 albedo, vec3 specColor, float shininess)
{
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * NdotL;
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specFactor = (NdotL > 0.0) ? pow(NdotH, shininess) : 0.0;
    vec3 specular = specColor * specFactor;
    return (diffuse + specular) * lightColor * intensity * attenuation;
}

float inverseSquareAttenuation(float dist, vec3 params) {
    return 1.0 / max(params.x + params.y * dist + params.z * dist * dist, 1e-4);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(scene.cameraPos.xyz - vWorldPos);
    vec3 albedo = vColor;
    vec3 specColor = ubo.specularColor.rgb;
    float shininess = max(ubo.specularColor.a, 1.0);

    vec3 result = albedo * scene.ambient.rgb * scene.ambient.a;

    // Directional
    {
        vec3 L = normalize(-scene.dirDirection.xyz);
        result += blinnPhong(N, V, L, scene.dirColor.rgb, scene.dirColor.a,
                             1.0, albedo, specColor, shininess);
    }

    // Point lights
    int pointCount = int(scene.lightCounts.x);
    for (int i = 0; i < pointCount; ++i) {
        vec3 lp = pointLights.lights[i].positionAndIntensity.xyz;
        float intensity = pointLights.lights[i].positionAndIntensity.w;
        if (intensity <= 0.0) continue;

        vec3 toLight = lp - vWorldPos;
        float dist = length(toLight);
        vec3 L = toLight / max(dist, 1e-4);
        float att = inverseSquareAttenuation(dist, pointLights.lights[i].params.xyz);

        result += blinnPhong(N, V, L, pointLights.lights[i].color.rgb, intensity,
                             att, albedo, specColor, shininess);
    }

    // Spot lights
    int spotCount = int(scene.lightCounts.y);
    for (int i = 0; i < spotCount; ++i) {
        vec3 lp = spotLights.lights[i].positionAndIntensity.xyz;
        float intensity = spotLights.lights[i].positionAndIntensity.w;
        if (intensity <= 0.0) continue;

        vec3 toLight = lp - vWorldPos;
        float dist = length(toLight);
        vec3 L = toLight / max(dist, 1e-4);

        vec3 spotToFrag = normalize(vWorldPos - lp);
        float cosAngle = dot(spotToFrag, normalize(spotLights.lights[i].direction.xyz));

        float cosInner = spotLights.lights[i].angles.x;
        float cosOuter = spotLights.lights[i].angles.y;

        float spotFactor = smoothstep(cosOuter, cosInner, cosAngle);
        if (spotFactor <= 0.0) continue;

        float att = inverseSquareAttenuation(dist, spotLights.lights[i].params.xyz);
        result += blinnPhong(N, V, L, spotLights.lights[i].color.rgb, intensity,
                             att * spotFactor, albedo, specColor, shininess);
    }

    outColor = vec4(result, ubo.albedo.a);
}