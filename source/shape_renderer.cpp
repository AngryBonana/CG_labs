#include "shape_renderer.hpp"

#include <cmath>
#include <array>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace shape_renderer {

namespace {

Resources res;

struct FaceInfo {
    uint32_t firstIndex;
    float distanceSq;
};


VkShaderModule loadShader(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "Cannot open shader: " << path << "\n";
        return VK_NULL_HANDLE;
    }
    size_t size = static_cast<size_t>(f.tellg());
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), static_cast<std::streamsize>(size));
    f.close();

    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = size;
    ci.pCode = reinterpret_cast<const uint32_t*>(buf.data());
    VkShaderModule m = VK_NULL_HANDLE;
    vkCreateShaderModule(graphics::internal::context.device, &ci, nullptr, &m);
    return m;
}

void createBuffer(VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkBuffer& buffer,
                  VmaAllocation& alloc,
                  void** mapped = nullptr) {
    VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    if (mapped) {
        ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                 | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo info{};
    vmaCreateBuffer(graphics::internal::context.allocator,
                    &bi, &ai, &buffer, &alloc, &info);
    if (mapped) {
        *mapped = info.pMappedData;
    }
}

} // namespace


bool initialize() {
    auto& ctx = graphics::internal::context;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslci.bindingCount = 1;
    dslci.pBindings = &uboBinding;
    if (vkCreateDescriptorSetLayout(ctx.device, &dslci, nullptr, &res.descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout\n";
        return false;
    }

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   Resources::MAX_OBJECTS };
    VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.maxSets = Resources::MAX_OBJECTS;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &res.descriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool\n";
        return false;
    }

    std::vector<VkDescriptorSetLayout> layouts(Resources::MAX_OBJECTS, res.descriptorSetLayout);

    VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsai.descriptorPool = res.descriptorPool;
    dsai.descriptorSetCount = Resources::MAX_OBJECTS;
    dsai.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptorSets(Resources::MAX_OBJECTS);
    if (vkAllocateDescriptorSets(ctx.device, &dsai, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets\n";
        return false;
    }

    for (int i = 0; i < Resources::MAX_OBJECTS; ++i) {
        auto& obj = res.objects[i];
        obj.descriptorSet = descriptorSets[i];

        createBuffer(sizeof(UniformBufferObject),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     obj.uniformBuffer, obj.uniformAlloc, &obj.uniformMapped);

        VkDescriptorBufferInfo dbi{ obj.uniformBuffer, 0, sizeof(UniformBufferObject) };
        VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        wds.dstSet = obj.descriptorSet;
        wds.dstBinding = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(ctx.device, 1, &wds, 0, nullptr);

        if (i > 0) {
            obj.position = glm::vec3(
                ((i == 1) ? 2.0f : (i == 2) ? -2.0f : 0.0f),
                0.0f,
                ((i == 3) ? 2.0f : 0.0f)
            );
            obj.baseColor = glm::vec3(
                (i == 1) ? 1.0f : 0.2f,
                (i == 2) ? 1.0f : 0.2f,
                (i == 3) ? 1.0f : 0.2f
            );
        }
    }

    MeshData mesh = makePyramid();
    res.indexCount = static_cast<uint32_t>(mesh.indices.size());

    VkDeviceSize vSize = sizeof(Vertex) * mesh.vertices.size();
    VkDeviceSize iSize = sizeof(uint16_t) * mesh.indices.size();

    VkBuffer vStage = VK_NULL_HANDLE, iStage = VK_NULL_HANDLE;
    VmaAllocation vStageAlloc = VK_NULL_HANDLE, iStageAlloc = VK_NULL_HANDLE;
    void* vStageMap = nullptr;
    void* iStageMap = nullptr;
    createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, vStage, vStageAlloc, &vStageMap);
    createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, iStage, iStageAlloc, &iStageMap);
    std::memcpy(vStageMap, mesh.vertices.data(), vSize);
    std::memcpy(iStageMap, mesh.indices.data(), iSize);

    createBuffer(vSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 res.vertexBuffer, res.vertexAlloc);
    createBuffer(iSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 res.indexBuffer, res.indexAlloc);

    VkCommandPoolCreateInfo cpci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.queueFamilyIndex = ctx.graphics_queue_index;
    VkCommandPool tmpPool = VK_NULL_HANDLE;
    vkCreateCommandPool(ctx.device, &cpci, nullptr, &tmpPool);

    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = tmpPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer tmpCmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(ctx.device, &cbai, &tmpCmd);

    VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tmpCmd, &cbbi);

    VkBufferCopy vCopy{ 0, 0, vSize };
    VkBufferCopy iCopy{ 0, 0, iSize };
    vkCmdCopyBuffer(tmpCmd, vStage, res.vertexBuffer, 1, &vCopy);
    vkCmdCopyBuffer(tmpCmd, iStage, res.indexBuffer, 1, &iCopy);
    vkEndCommandBuffer(tmpCmd);

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &tmpCmd;
    vkQueueSubmit(ctx.graphics_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphics_queue);

    vkDestroyCommandPool(ctx.device, tmpPool, nullptr);
    vmaDestroyBuffer(ctx.allocator, vStage, vStageAlloc);
    vmaDestroyBuffer(ctx.allocator, iStage, iStageAlloc);

    VkShaderModule vert = loadShader("shaders/shape.vert.spv");
    VkShaderModule frag = loadShader("shaders/shape.frag.spv");
    if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    auto binding = Vertex::bindingDescription();
    auto attrs = Vertex::attributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vici{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vici.vertexBindingDescriptionCount = 1;
    vici.pVertexBindingDescriptions = &binding;
    vici.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vici.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo iaci{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaci.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo vsci{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vsci.viewportCount = 1;
    vsci.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rsci{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsci.polygonMode = VK_POLYGON_MODE_FILL;
    rsci.cullMode = VK_CULL_MODE_NONE;
    rsci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msci{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsci{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsci.depthTestEnable = VK_TRUE;
    dsci.depthWriteEnable = VK_FALSE;
    dsci.depthCompareOp = VK_COMPARE_OP_LESS;
    dsci.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbas{};
    cbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbas.blendEnable = VK_TRUE;
    cbas.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbas.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbas.colorBlendOp        = VK_BLEND_OP_ADD;
    cbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbas.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cbci{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbci.attachmentCount = 1;
    cbci.pAttachments = &cbas;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dssci{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dssci.dynamicStateCount = 2;
    dssci.pDynamicStates = dynStates;

    VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &res.descriptorSetLayout;
    if (vkCreatePipelineLayout(ctx.device, &plci, nullptr, &res.pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout\n";
        return false;
    }

    VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vici;
    gpci.pInputAssemblyState = &iaci;
    gpci.pViewportState = &vsci;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState = &msci;
    gpci.pDepthStencilState = &dsci;
    gpci.pColorBlendState = &cbci;
    gpci.pDynamicState = &dssci;
    gpci.layout = res.pipelineLayout;
    gpci.renderPass = ctx.render_pass;
    gpci.subpass = 0;

    if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &res.pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline\n";
        return false;
    }

    vkDestroyShaderModule(ctx.device, vert, nullptr);
    vkDestroyShaderModule(ctx.device, frag, nullptr);

    return true;
}

void update(double dt) {
    auto& ctx = graphics::internal::context;

    for (int i = 0; i < res.objectCount; ++i) {
        auto& obj = res.objects[i];
        if (obj.animating) {
            obj.animTime += dt * static_cast<double>(obj.animSpeed);
        }
    }

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, res.cameraDistance),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    float aspect = static_cast<float>(ctx.swapchain_extent.width) /
                   static_cast<float>(ctx.swapchain_extent.height);

    glm::mat4 proj;
    if (res.usePerspective) {
        proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    } else {
        const float orthoHalfH = 2.0f;
        const float orthoHalfW = orthoHalfH * aspect;
        proj = glm::ortho(-orthoHalfW, orthoHalfW,
                          -orthoHalfH, orthoHalfH,
                          0.1f, 100.0f);
    }

    const glm::mat4 vulkanCorrection(
        glm::vec4(1.0f,  0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f,  0.0f, 0.5f, 0.0f),
        glm::vec4(0.0f,  0.0f, 0.5f, 1.0f)
    );
    proj = vulkanCorrection * proj;

    for (int i = 0; i < res.objectCount; ++i) {
        auto& obj = res.objects[i];
        UniformBufferObject ubo{};

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, obj.position);

        if (obj.animating || obj.animTime > 0.0) {
            float t = static_cast<float>(obj.animTime);
            float orbitX = std::cos(t) * obj.pathRadius;
            float orbitZ = std::sin(t) * obj.pathRadius;

            const float blendTime = 1.0f;
            float blend = std::min(t / blendTime, 1.0f);

            model = glm::translate(model, glm::vec3(orbitX * blend, 0.0f, orbitZ * blend));
        }

        model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        if (obj.animating || obj.animTime > 0.0) {
            model = glm::rotate(model, static_cast<float>(obj.animTime), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        model = glm::scale(model, obj.scale);
        obj.lastModel = model;

        ubo.model = model;
        ubo.view = view;
        ubo.proj = proj;
        ubo.baseColor = glm::vec4(obj.baseColor, obj.alpha);

        std::memcpy(obj.uniformMapped, &ubo, sizeof(ubo));
    }
}

void record(VkCommandBuffer cmd, uint32_t imageIndex) {
    (void)imageIndex;
    auto& ctx = graphics::internal::context;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.pipeline);

    VkViewport vp{ 0.0f, 0.0f,
                   static_cast<float>(ctx.swapchain_extent.width),
                   static_cast<float>(ctx.swapchain_extent.height),
                   0.0f, 1.0f };
    VkRect2D sc{ {0, 0}, ctx.swapchain_extent };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &res.vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cmd, res.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    const glm::vec3 localVerts[4] = {
        { 0.0f, 1.0f, 0.0f },
        { 0.9428f, -1.0f/3.0f, 0.0f },
        { -0.4714f, -1.0f/3.0f, 0.8165f },
        { -0.4714f, -1.0f/3.0f, -0.8165f },
    };
    const uint32_t faceIndices[4][3] = {
        { 0, 2, 1 },
        { 0, 3, 2 },
        { 0, 1, 3 },
        { 1, 2, 3 },
    };

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, res.cameraDistance);

    struct ObjInfo {
        int index;
        float distanceSq;
    };
    ObjInfo objOrder[Resources::MAX_OBJECTS];
    for (int i = 0; i < res.objectCount; ++i) {
        glm::vec3 worldPos = glm::vec3(res.objects[i].lastModel *
                                       glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 d = worldPos - cameraPos;
        objOrder[i].index = i;
        objOrder[i].distanceSq = glm::dot(d, d);
    }

    std::sort(objOrder, objOrder + res.objectCount,
              [](const ObjInfo& a, const ObjInfo& b) {
                  return a.distanceSq > b.distanceSq;
              });

    for (int k = 0; k < res.objectCount; ++k) {
        int i = objOrder[k].index;
        auto& obj = res.objects[i];

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                res.pipelineLayout, 0, 1, &obj.descriptorSet, 0, nullptr);

        FaceInfo faces[4];
        for (int f = 0; f < 4; ++f) {
            glm::vec3 v0 = glm::vec3(obj.lastModel * glm::vec4(localVerts[faceIndices[f][0]], 1.0f));
            glm::vec3 v1 = glm::vec3(obj.lastModel * glm::vec4(localVerts[faceIndices[f][1]], 1.0f));
            glm::vec3 v2 = glm::vec3(obj.lastModel * glm::vec4(localVerts[faceIndices[f][2]], 1.0f));
            glm::vec3 center = (v0 + v1 + v2) / 3.0f;
            glm::vec3 d = center - cameraPos;
            faces[f].firstIndex = f * 3;
            faces[f].distanceSq = glm::dot(d, d);
        }

        std::sort(faces, faces + 4,
                  [](const FaceInfo& a, const FaceInfo& b) {
                      return a.distanceSq > b.distanceSq;
                  });

        for (int f = 0; f < 4; ++f) {
            vkCmdDrawIndexed(cmd, 3, 1, faces[f].firstIndex, 0, 0);
        }
    }
}

void shutdown() {
    auto& ctx = graphics::internal::context;
    vkDeviceWaitIdle(ctx.device);

    if (res.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx.device, res.pipeline, nullptr);
        res.pipeline = VK_NULL_HANDLE;
    }
    if (res.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx.device, res.pipelineLayout, nullptr);
        res.pipelineLayout = VK_NULL_HANDLE;
    }
    if (res.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device, res.descriptorPool, nullptr);
        res.descriptorPool = VK_NULL_HANDLE;
    }
    if (res.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device, res.descriptorSetLayout, nullptr);
        res.descriptorSetLayout = VK_NULL_HANDLE;
    }

    for (int i = 0; i < Resources::MAX_OBJECTS; ++i) {
        auto& obj = res.objects[i];
        if (obj.uniformBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(ctx.allocator, obj.uniformBuffer, obj.uniformAlloc);
            obj.uniformBuffer = VK_NULL_HANDLE;
            obj.uniformMapped = nullptr;
            obj.descriptorSet = VK_NULL_HANDLE;
        }
    }

    if (res.vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator, res.vertexBuffer, res.vertexAlloc);
        res.vertexBuffer = VK_NULL_HANDLE;
    }
    if (res.indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator, res.indexBuffer, res.indexAlloc);
        res.indexBuffer = VK_NULL_HANDLE;
    }
}

Resources& params() {
    return res;
}

} // namespace shape_renderer