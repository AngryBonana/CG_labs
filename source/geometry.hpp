#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static VkVertexInputBindingDescription bindingDescription() {
        VkVertexInputBindingDescription b{};
        b.binding = 0;
        b.stride = sizeof(Vertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }

    static std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> a{};
        a[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
        a[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
        a[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)};
        a[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
        return a;
    }
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

inline void computeNormals(MeshData& m) {
    for (auto& v : m.vertices) v.normal = glm::vec3(0.0f);
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        uint16_t i0 = m.indices[i + 0];
        uint16_t i1 = m.indices[i + 1];
        uint16_t i2 = m.indices[i + 2];
        glm::vec3 e1 = m.vertices[i1].pos - m.vertices[i0].pos;
        glm::vec3 e2 = m.vertices[i2].pos - m.vertices[i0].pos;
        glm::vec3 n = glm::cross(e1, e2);
        m.vertices[i0].normal += n;
        m.vertices[i1].normal += n;
        m.vertices[i2].normal += n;
    }
    for (auto& v : m.vertices) {
        float len = glm::length(v.normal);
        v.normal = (len > 1e-6f) ? v.normal / len : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

inline MeshData makePyramid() {
    const float r = 1.0f;

    glm::vec3 P[4] = {
        {0.0f, r, 0.0f},
        { 0.9428f * r, -r / 3.0f, 0.0f},
        {-0.4714f * r, -r / 3.0f, 0.8165f * r},
        {-0.4714f * r, -r / 3.0f, -0.8165f * r},
    };

    glm::vec3 center = (P[0] + P[1] + P[2] + P[3]) * 0.25f;

    const int faces[4][3] = {
        {0, 1, 2},
        {0, 2, 3},
        {0, 3, 1},
        {1, 2, 3},
    };

    glm::vec3 N[4] = {glm::vec3(0.0f)};
    for (int f = 0; f < 4; ++f) {
        const glm::vec3& a = P[faces[f][0]];
        const glm::vec3& b = P[faces[f][1]];
        const glm::vec3& c = P[faces[f][2]];
        glm::vec3 n = glm::cross(b - a, c - a);
        float len = glm::length(n);
        if (len > 1e-6f) n /= len;

        glm::vec3 faceCenter = (a + b + c) / 3.0f;
        if (glm::dot(n, faceCenter - center) < 0.0f) {
            n = -n;
        }

        N[faces[f][0]] += n;
        N[faces[f][1]] += n;
        N[faces[f][2]] += n;
    }
    for (int i = 0; i < 4; ++i) {
        float len = glm::length(N[i]);
        N[i] = (len > 1e-6f) ? N[i] / len : glm::vec3(0, 1, 0);
    }

    MeshData m;
    auto addFace = [&](int i0, int i1, int i2) {
        uint16_t base = static_cast<uint16_t>(m.vertices.size());
        m.vertices.push_back({P[i0], N[i0], {1,1,1}, {0.0f, 1.0f}});
        m.vertices.push_back({P[i1], N[i1], {1,1,1}, {0.0f, 0.0f}});
        m.vertices.push_back({P[i2], N[i2], {1,1,1}, {1.0f, 0.0f}});
        m.indices.push_back(base);
        m.indices.push_back(base + 1);
        m.indices.push_back(base + 2);
    };

    addFace(0, 1, 2);
    addFace(0, 2, 3);
    addFace(0, 3, 1);
    addFace(1, 2, 3);

    return m;
}

inline MeshData makeCube() {
    MeshData m;
    glm::vec3 p[8] = {
        {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
        {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}
    };
    for (auto& v : p) m.vertices.push_back({v, {0,0,0}, {1,1,1}, {0.5f, 0.5f}});
    m.indices = {
        0,1,2, 0,2,3,
        4,6,5, 4,7,6,
        0,4,5, 0,5,1,
        3,2,6, 3,6,7,
        0,3,7, 0,7,4,
        1,5,6, 1,6,2
    };
    computeNormals(m);
    for (auto& v : m.vertices) v.uv = glm::vec2(0.5f);
    return m;
}

inline std::vector<Vertex> makeSpotConeLines(const glm::vec3& pos, const glm::vec3& dir,
                                             float outerRad, float length = 2.5f, int segments = 16) {
    std::vector<Vertex> out;
    out.reserve(static_cast<size_t>(segments) * 4);

    glm::vec3 d = glm::normalize(dir);
    glm::vec3 up = (std::abs(d.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                           : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right  = glm::normalize(glm::cross(up, d));
    glm::vec3 realUp = glm::cross(d, right);

    float radius = std::tan(outerRad) * length;
    glm::vec3 center = pos + d * length;

    std::vector<glm::vec3> circle(static_cast<size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        float a = (2.0f * 3.14159265358979f * static_cast<float>(i)) /
                  static_cast<float>(segments);
        circle[static_cast<size_t>(i)] =
            center + right * (std::cos(a) * radius) +
                     realUp * (std::sin(a) * radius);
    }

    Vertex v{};
    v.color = glm::vec3(1.0f);
    v.uv = glm::vec2(0.0f);
    for (int i = 0; i < segments; ++i) {
        const glm::vec3& c0 = circle[static_cast<size_t>(i)];
        const glm::vec3& c1 = circle[static_cast<size_t>((i + 1) % segments)];

        v.pos = pos;      out.push_back(v);
        v.pos = c0;       out.push_back(v);
        v.pos = c0;       out.push_back(v);
        v.pos = c1;       out.push_back(v);
    }
    return out;
}

inline MeshData makePlane(float halfSize = 5.0f) {
    MeshData m;
    m.vertices = {
        {{-halfSize, 0.0f, -halfSize}, {0,1,0}, {1,1,1}, {0.0f, 0.0f}},
        {{ halfSize, 0.0f, -halfSize}, {0,1,0}, {1,1,1}, {1.0f, 0.0f}},
        {{ halfSize, 0.0f,  halfSize}, {0,1,0}, {1,1,1}, {1.0f, 1.0f}},
        {{-halfSize, 0.0f,  halfSize}, {0,1,0}, {1,1,1}, {0.0f, 1.0f}},
    };
    m.indices = { 0, 1, 2, 0, 2, 3 };
    return m;
}