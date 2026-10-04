#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription bindingDescription() {
        VkVertexInputBindingDescription b{};
        b.binding = 0;
        b.stride = sizeof(Vertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }

    static std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> a{};
        a[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
        a[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)};
        return a;
    }
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

// Правильный тетраэдр (пирамида)
inline MeshData makePyramid() {
    const float r = 1.0f;
    MeshData m;
    m.vertices = {
        {{0.0f, r, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.9428f * r, -r / 3.0f,  0.0f }, {0.0f, 1.0f, 0.0f}},
        {{-0.4714f * r, -r / 3.0f, 0.8165f * r }, {0.0f, 0.0f, 1.0f}},
        {{-0.4714f * r, -r / 3.0f, -0.8165f * r }, {1.0f, 1.0f, 0.0f}},
    };
    m.indices = {
        0, 2, 1,
        0, 3, 2,
        0, 1, 3,
        1, 2, 3
    };
    return m;
}