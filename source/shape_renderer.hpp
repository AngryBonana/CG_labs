#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <vector>

#include "graphics_internal.hpp"
#include "geometry.hpp"
#include "uniform.hpp"

namespace shape_renderer
{

    struct GameObject
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};

        glm::vec3 albedo{1.0f, 0.6f, 0.2f};
        float alpha = 1.0f;
        glm::vec3 specularColor{1.0f};
        float shininess = 32.0f;

        bool animating = false;
        float animSpeed = 1.0f;
        float pathRadius = 1.5f;
        double animTime = 0.0;

        VkBuffer uniformBuffer = VK_NULL_HANDLE;
        VmaAllocation uniformAlloc = VK_NULL_HANDLE;
        void *uniformMapped = nullptr;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        glm::mat4 lastModel{1.0f};
    };

    struct PointLightData
    {
        glm::vec3 position{0.0f};
        float intensity = 1.0f;
        glm::vec3 color{1.0f};
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    };

    struct SpotLightData
    {
        glm::vec3 position{0.0f, 3.0f, 0.0f};
        float intensity = 1.0f;
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        glm::vec3 color{1.0f};
        float innerAngleDeg = 15.0f;
        float outerAngleDeg = 25.0f;
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    };

    struct Camera
    {
        float yaw = 0.0f;
        float pitch = 15.0f;
        float distance = 6.0f;
        float fovDeg = 45.0f;
        glm::vec3 target{0.0f};

        bool usePerspective = true;
        float orthoHalfHeight = 3.0f;
    };

    struct Resources
    {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipeline transparentPipeline = VK_NULL_HANDLE;

        VkPipelineLayout visPipelineLayout = VK_NULL_HANDLE;
        VkPipeline visTriPipeline = VK_NULL_HANDLE;
        VkPipeline visLinePipeline = VK_NULL_HANDLE;

        VkDescriptorSetLayout sceneSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool scenePool = VK_NULL_HANDLE;
        VkDescriptorSet sceneSet = VK_NULL_HANDLE;

        VkBuffer sceneUBO = VK_NULL_HANDLE;
        VmaAllocation sceneUBOAlloc = VK_NULL_HANDLE;
        void *sceneUBOMapped = nullptr;

        VkBuffer pointSSBO = VK_NULL_HANDLE;
        VmaAllocation pointSSBOAlloc = VK_NULL_HANDLE;
        void *pointSSBOMapped = nullptr;
        VkDeviceSize pointSSBOSize = 0;

        VkBuffer spotSSBO = VK_NULL_HANDLE;
        VmaAllocation spotSSBOAlloc = VK_NULL_HANDLE;
        void *spotSSBOMapped = nullptr;
        VkDeviceSize spotSSBOSize = 0;

        VkDescriptorSetLayout modelSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool modelPool = VK_NULL_HANDLE;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VmaAllocation vertexAlloc = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VmaAllocation indexAlloc = VK_NULL_HANDLE;
        uint32_t indexCount = 0;

        VkBuffer cubeVertexBuffer = VK_NULL_HANDLE;
        VmaAllocation cubeVertexAlloc = VK_NULL_HANDLE;
        VkBuffer cubeIndexBuffer = VK_NULL_HANDLE;
        VmaAllocation cubeIndexAlloc = VK_NULL_HANDLE;
        uint32_t cubeIndexCount = 0;

        VkBuffer coneVertexBuffer = VK_NULL_HANDLE;
        VmaAllocation coneVertexAlloc = VK_NULL_HANDLE;
        void *coneVertexMapped = nullptr;
        uint32_t coneVertexCapacity = 0;
        uint32_t coneVertexWritten = 0;

        static constexpr int MAX_OBJECTS = 3;
        GameObject objects[MAX_OBJECTS];
        int objectCount = 1;
        int selectedObject = 0;

        Camera camera;
        glm::vec3 cameraEye{0.0f};

        glm::vec3 dirLightDirection{-0.5f, -1.0f, -0.3f};
        glm::vec3 dirLightColor{1.0f};
        float dirLightIntensity = 1.0f;

        glm::vec3 ambientColor{1.0f};
        float ambientIntensity = 0.15f;

        std::vector<PointLightData> pointLights;
        std::vector<SpotLightData> spotLights;

        bool showLightGizmos = true;

        static constexpr uint32_t MAX_LIGHTS = 64;
    };

    bool initialize();
    void shutdown();
    void update(double dt);
    void record(VkCommandBuffer cmd, uint32_t imageIndex);
    Resources &params();

} // namespace shape_renderer