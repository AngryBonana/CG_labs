#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

#include "graphics_internal.hpp"
#include "geometry.hpp"
#include "uniform.hpp"

namespace shape_renderer {

struct GameObject {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 baseColor{1.0f, 0.6f, 0.2f};
    float alpha = 1.0f;

    bool animating = false;
    float animSpeed = 1.0f;
    float pathRadius = 1.5f;
    double animTime = 0.0;

    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VmaAllocation uniformAlloc = VK_NULL_HANDLE;
    void* uniformMapped = nullptr;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    glm::mat4 lastModel{1.0f};
};

struct Resources {
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAlloc = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAlloc = VK_NULL_HANDLE;
    uint32_t indexCount = 0;

    static constexpr int MAX_OBJECTS = 3;
    GameObject objects[MAX_OBJECTS];
    int objectCount = 1;
    int selectedObject = 0;

    bool usePerspective = true;
    float cameraDistance = 6.0f;
};

bool initialize();
void shutdown();
void update(double dt);
void record(VkCommandBuffer cmd, uint32_t imageIndex);

Resources& params();

} // namespace shape_renderer