#pragma once

#include <glm/glm.hpp>


struct SceneUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 cameraPos;
    alignas(16) glm::vec4 ambient;

    alignas(16) glm::vec4 dirDirection;
    alignas(16) glm::vec4 dirColor;

    alignas(16) glm::vec4 lightCounts;
};


struct PointLight {
    alignas(16) glm::vec4 position;
    alignas(16) glm::vec4 color;
    alignas(16) glm::vec4 params;
};


struct SpotLight {
    alignas(16) glm::vec4 position;
    alignas(16) glm::vec4 direction;
    alignas(16) glm::vec4 color;
    alignas(16) glm::vec4 angles;
    alignas(16) glm::vec4 params;
};


struct ModelUBO {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 normalMatrix;
    alignas(16) glm::vec4 albedo;
    alignas(16) glm::vec4 specularColor;
};