#include "shape_renderer.hpp"

#include <cmath>
#include <array>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace shape_renderer
{

    namespace
    {

        Resources res;

        VkShaderModule loadShader(const std::string &path)
        {
            std::ifstream f(path, std::ios::ate | std::ios::binary);
            if (!f.is_open())
            {
                std::cerr << "Cannot open shader: " << path << "\n";
                return VK_NULL_HANDLE;
            }
            size_t size = static_cast<size_t>(f.tellg());
            std::vector<char> buf(size);
            f.seekg(0);
            f.read(buf.data(), static_cast<std::streamsize>(size));
            f.close();

            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize = size;
            ci.pCode = reinterpret_cast<const uint32_t *>(buf.data());
            VkShaderModule m = VK_NULL_HANDLE;
            vkCreateShaderModule(graphics::internal::context.device, &ci, nullptr, &m);
            return m;
        }

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkBuffer &buffer, VmaAllocation &alloc, void **mapped = nullptr)
        {
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bi.size = size;
            bi.usage = usage;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_AUTO;
            if (mapped)
            {
                ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            }
            VmaAllocationInfo info{};
            vmaCreateBuffer(graphics::internal::context.allocator, &bi, &ai, &buffer, &alloc, &info);
            if (mapped)
                *mapped = info.pMappedData;
        }

        void uploadMesh(const MeshData &mesh,
                        VkBuffer &vBuf, VmaAllocation &vAlloc,
                        VkBuffer &iBuf, VmaAllocation &iAlloc)
        {
            auto &ctx = graphics::internal::context;
            VkDeviceSize vSize = sizeof(Vertex) * mesh.vertices.size();
            VkDeviceSize iSize = sizeof(uint16_t) * mesh.indices.size();

            VkBuffer vStage = VK_NULL_HANDLE, iStage = VK_NULL_HANDLE;
            VmaAllocation vStageAlloc = VK_NULL_HANDLE, iStageAlloc = VK_NULL_HANDLE;
            void *vStageMap = nullptr;
            void *iStageMap = nullptr;

            createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, vStage, vStageAlloc, &vStageMap);
            createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, iStage, iStageAlloc, &iStageMap);
            std::memcpy(vStageMap, mesh.vertices.data(), vSize);
            std::memcpy(iStageMap, mesh.indices.data(), iSize);

            createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vBuf, vAlloc);
            createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         iBuf, iAlloc);

            VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            cpci.queueFamilyIndex = ctx.graphics_queue_index;
            VkCommandPool tmpPool = VK_NULL_HANDLE;
            vkCreateCommandPool(ctx.device, &cpci, nullptr, &tmpPool);

            VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cbai.commandPool = tmpPool;
            cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbai.commandBufferCount = 1;
            VkCommandBuffer tmpCmd = VK_NULL_HANDLE;
            vkAllocateCommandBuffers(ctx.device, &cbai, &tmpCmd);

            VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(tmpCmd, &cbbi);
            VkBufferCopy vCopy{0, 0, vSize};
            VkBufferCopy iCopy{0, 0, iSize};
            vkCmdCopyBuffer(tmpCmd, vStage, vBuf, 1, &vCopy);
            vkCmdCopyBuffer(tmpCmd, iStage, iBuf, 1, &iCopy);
            vkEndCommandBuffer(tmpCmd);

            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers = &tmpCmd;
            vkQueueSubmit(ctx.graphics_queue, 1, &si, VK_NULL_HANDLE);
            vkQueueWaitIdle(ctx.graphics_queue);

            vkDestroyCommandPool(ctx.device, tmpPool, nullptr);
            vmaDestroyBuffer(ctx.allocator, vStage, vStageAlloc);
            vmaDestroyBuffer(ctx.allocator, iStage, iStageAlloc);
        }

    } // namespace


    bool initialize()
    {
        auto &ctx = graphics::internal::context;

        VkDescriptorSetLayoutBinding sceneBindings[3]{};
        sceneBindings[0].binding = 0;
        sceneBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sceneBindings[0].descriptorCount = 1;
        sceneBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        sceneBindings[1].binding = 1;
        sceneBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sceneBindings[1].descriptorCount = 1;
        sceneBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        sceneBindings[2].binding = 2;
        sceneBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sceneBindings[2].descriptorCount = 1;
        sceneBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslci.bindingCount = 3;
        dslci.pBindings = sceneBindings;
        if (vkCreateDescriptorSetLayout(ctx.device, &dslci, nullptr, &res.sceneSetLayout) != VK_SUCCESS)
        {
            std::cerr << "Failed to create scene descriptor set layout\n";
            return false;
        }

        VkDescriptorSetLayoutBinding modelBinding{};
        modelBinding.binding = 0;
        modelBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        modelBinding.descriptorCount = 1;
        modelBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo mdslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        mdslci.bindingCount = 1;
        mdslci.pBindings = &modelBinding;
        if (vkCreateDescriptorSetLayout(ctx.device, &mdslci, nullptr, &res.modelSetLayout) != VK_SUCCESS)
        {
            std::cerr << "Failed to create model descriptor set layout\n";
            return false;
        }

        createBuffer(sizeof(SceneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     res.sceneUBO, res.sceneUBOAlloc, &res.sceneUBOMapped);

        res.pointSSBOSize = sizeof(PointLight) * Resources::MAX_LIGHTS;
        res.spotSSBOSize = sizeof(SpotLight) * Resources::MAX_LIGHTS;

        createBuffer(res.pointSSBOSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     res.pointSSBO, res.pointSSBOAlloc, &res.pointSSBOMapped);
        createBuffer(res.spotSSBOSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     res.spotSSBO, res.spotSSBOAlloc, &res.spotSSBOMapped);

        VkDescriptorPoolSize scenePoolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        };
        VkDescriptorPoolCreateInfo spci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        spci.maxSets = 1;
        spci.poolSizeCount = 2;
        spci.pPoolSizes = scenePoolSizes;
        if (vkCreateDescriptorPool(ctx.device, &spci, nullptr, &res.scenePool) != VK_SUCCESS)
        {
            std::cerr << "Failed to create scene descriptor pool\n";
            return false;
        }

        VkDescriptorSetAllocateInfo sadai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        sadai.descriptorPool = res.scenePool;
        sadai.descriptorSetCount = 1;
        sadai.pSetLayouts = &res.sceneSetLayout;
        if (vkAllocateDescriptorSets(ctx.device, &sadai, &res.sceneSet) != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate scene descriptor set\n";
            return false;
        }

        VkDescriptorBufferInfo sceneUBOInfo{res.sceneUBO, 0, sizeof(SceneUBO)};
        VkDescriptorBufferInfo pointInfo{res.pointSSBO, 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo spotInfo{res.spotSSBO, 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet sceneWrites[3]{};
        sceneWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sceneWrites[0].dstSet = res.sceneSet;
        sceneWrites[0].dstBinding = 0;
        sceneWrites[0].descriptorCount = 1;
        sceneWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sceneWrites[0].pBufferInfo = &sceneUBOInfo;

        sceneWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sceneWrites[1].dstSet = res.sceneSet;
        sceneWrites[1].dstBinding = 1;
        sceneWrites[1].descriptorCount = 1;
        sceneWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sceneWrites[1].pBufferInfo = &pointInfo;

        sceneWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sceneWrites[2].dstSet = res.sceneSet;
        sceneWrites[2].dstBinding = 2;
        sceneWrites[2].descriptorCount = 1;
        sceneWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sceneWrites[2].pBufferInfo = &spotInfo;

        vkUpdateDescriptorSets(ctx.device, 3, sceneWrites, 0, nullptr);

        // ---------- Model descriptor pool/set ----------
        VkDescriptorPoolSize modelPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           Resources::MAX_OBJECTS};
        VkDescriptorPoolCreateInfo mpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        mpci.maxSets = Resources::MAX_OBJECTS;
        mpci.poolSizeCount = 1;
        mpci.pPoolSizes = &modelPoolSize;
        if (vkCreateDescriptorPool(ctx.device, &mpci, nullptr, &res.modelPool) != VK_SUCCESS)
        {
            std::cerr << "Failed to create model descriptor pool\n";
            return false;
        }

        std::vector<VkDescriptorSetLayout> layouts(Resources::MAX_OBJECTS, res.modelSetLayout);
        VkDescriptorSetAllocateInfo mdai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        mdai.descriptorPool = res.modelPool;
        mdai.descriptorSetCount = Resources::MAX_OBJECTS;
        mdai.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> modelSets(Resources::MAX_OBJECTS);
        if (vkAllocateDescriptorSets(ctx.device, &mdai, modelSets.data()) != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate model descriptor sets\n";
            return false;
        }

        for (int i = 0; i < Resources::MAX_OBJECTS; ++i)
        {
            auto &obj = res.objects[i];
            obj.descriptorSet = modelSets[i];

            createBuffer(sizeof(ModelUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         obj.uniformBuffer, obj.uniformAlloc, &obj.uniformMapped);

            VkDescriptorBufferInfo dbi{obj.uniformBuffer, 0, sizeof(ModelUBO)};
            VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wds.dstSet = obj.descriptorSet;
            wds.dstBinding = 0;
            wds.descriptorCount = 1;
            wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wds.pBufferInfo = &dbi;
            vkUpdateDescriptorSets(ctx.device, 1, &wds, 0, nullptr);

            if (i > 0)
            {
                obj.position = glm::vec3(
                    (i == 1) ? 2.0f : (i == 2) ? -2.0f
                                               : 0.0f,
                    0.0f,
                    (i == 3) ? 2.0f : 0.0f);
                obj.albedo = glm::vec3(
                    (i == 1) ? 1.0f : 0.2f,
                    (i == 2) ? 1.0f : 0.2f,
                    (i == 3) ? 1.0f : 0.2f);
            }
        }

        MeshData mesh = makePyramid();
        res.indexCount = static_cast<uint32_t>(mesh.indices.size());
        uploadMesh(mesh, res.vertexBuffer, res.vertexAlloc, res.indexBuffer, res.indexAlloc);

        MeshData cube = makeCube();
        res.cubeIndexCount = static_cast<uint32_t>(cube.indices.size());
        uploadMesh(cube, res.cubeVertexBuffer, res.cubeVertexAlloc,
                   res.cubeIndexBuffer, res.cubeIndexAlloc);

        const uint32_t coneVertsPerSpot = 16u * 4u;
        res.coneVertexCapacity = coneVertsPerSpot * Resources::MAX_LIGHTS;
        res.coneVertexWritten = 0;
        createBuffer(sizeof(Vertex) * res.coneVertexCapacity,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     res.coneVertexBuffer, res.coneVertexAlloc,
                     &res.coneVertexMapped);

        VkShaderModule vert = loadShader("shaders/shape.vert.spv");
        VkShaderModule frag = loadShader("shaders/shape.frag.spv");
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
            return false;

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

        VkPipelineVertexInputStateCreateInfo vici{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vici.vertexBindingDescriptionCount = 1;
        vici.pVertexBindingDescriptions = &binding;
        vici.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vici.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo iaci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vsci{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vsci.viewportCount = 1;
        vsci.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rsci{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rsci.polygonMode = VK_POLYGON_MODE_FILL;
        rsci.cullMode = VK_CULL_MODE_NONE;
        rsci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsci.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo msci{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState cbas{};
        cbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cbas.blendEnable = VK_TRUE;
        cbas.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbas.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbas.colorBlendOp = VK_BLEND_OP_ADD;
        cbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo cbci{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbci.attachmentCount = 1;
        cbci.pAttachments = &cbas;

        VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dssci{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dssci.dynamicStateCount = 2;
        dssci.pDynamicStates = dynStates;

        VkDescriptorSetLayout setLayouts[2] = {res.sceneSetLayout, res.modelSetLayout};
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 2;
        plci.pSetLayouts = setLayouts;
        if (vkCreatePipelineLayout(ctx.device, &plci, nullptr, &res.pipelineLayout) != VK_SUCCESS)
        {
            std::cerr << "Failed to create pipeline layout\n";
            return false;
        }

        auto createShapePipeline = [&](VkBool32 depthWrite) -> VkPipeline
        {
            VkPipelineDepthStencilStateCreateInfo dsci{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            dsci.depthTestEnable = VK_TRUE;
            dsci.depthWriteEnable = depthWrite;
            dsci.depthCompareOp = VK_COMPARE_OP_LESS;
            dsci.stencilTestEnable = VK_FALSE;

            VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
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

            VkPipeline p = VK_NULL_HANDLE;
            if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &p) != VK_SUCCESS)
            {
                std::cerr << "Failed to create shape pipeline (depthWrite=" << depthWrite << ")\n";
                return VK_NULL_HANDLE;
            }
            return p;
        };

        res.pipeline = createShapePipeline(VK_TRUE);
        res.transparentPipeline = createShapePipeline(VK_FALSE);
        if (res.pipeline == VK_NULL_HANDLE || res.transparentPipeline == VK_NULL_HANDLE)
        {
            return false;
        }

        vkDestroyShaderModule(ctx.device, vert, nullptr);
        vkDestroyShaderModule(ctx.device, frag, nullptr);

        VkShaderModule vvert = loadShader("shaders/vis.vert.spv");
        VkShaderModule vfrag = loadShader("shaders/vis.frag.spv");
        if (vvert == VK_NULL_HANDLE || vfrag == VK_NULL_HANDLE)
            return false;

        VkPipelineShaderStageCreateInfo vstages[2]{};
        vstages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vstages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        vstages[0].module = vvert;
        vstages[0].pName = "main";
        vstages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vstages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        vstages[1].module = vfrag;
        vstages[1].pName = "main";

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);

        VkPipelineLayoutCreateInfo vplci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        vplci.setLayoutCount = 1;
        vplci.pSetLayouts = &res.sceneSetLayout;
        vplci.pushConstantRangeCount = 1;
        vplci.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(ctx.device, &vplci, nullptr, &res.visPipelineLayout) != VK_SUCCESS)
        {
            std::cerr << "Failed to create vis pipeline layout\n";
            return false;
        }

        VkPipelineDepthStencilStateCreateInfo vdsci{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        vdsci.depthTestEnable = VK_TRUE;
        vdsci.depthWriteEnable = VK_FALSE;
        vdsci.depthCompareOp = VK_COMPARE_OP_LESS;
        vdsci.stencilTestEnable = VK_FALSE;

        VkPipelineInputAssemblyStateCreateInfo viaci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        viaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkGraphicsPipelineCreateInfo vgpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        vgpci.stageCount = 2;
        vgpci.pStages = vstages;
        vgpci.pVertexInputState = &vici;
        vgpci.pInputAssemblyState = &viaci;
        vgpci.pViewportState = &vsci;
        vgpci.pRasterizationState = &rsci;
        vgpci.pMultisampleState = &msci;
        vgpci.pDepthStencilState = &vdsci;
        vgpci.pColorBlendState = &cbci;
        vgpci.pDynamicState = &dssci;
        vgpci.layout = res.visPipelineLayout;
        vgpci.renderPass = ctx.render_pass;
        vgpci.subpass = 0;

        if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &vgpci, nullptr, &res.visTriPipeline) != VK_SUCCESS)
        {
            std::cerr << "Failed to create vis tri pipeline\n";
            return false;
        }

        VkPipelineInputAssemblyStateCreateInfo viaci2 = viaci;
        viaci2.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        vgpci.pInputAssemblyState = &viaci2;
        if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &vgpci, nullptr, &res.visLinePipeline) != VK_SUCCESS)
        {
            std::cerr << "Failed to create vis line pipeline\n";
            return false;
        }

        vkDestroyShaderModule(ctx.device, vvert, nullptr);
        vkDestroyShaderModule(ctx.device, vfrag, nullptr);

        return true;
    }


    void update(double dt)
    {
        auto &ctx = graphics::internal::context;

        for (int i = 0; i < res.objectCount; ++i)
        {
            auto &obj = res.objects[i];
            if (obj.animating)
                obj.animTime += dt * static_cast<double>(obj.animSpeed);
        }

        float yawRad = glm::radians(res.camera.yaw);
        float pitchRad = glm::radians(glm::clamp(res.camera.pitch, -89.0f, 89.0f));
        glm::vec3 dir(
            std::cos(pitchRad) * std::sin(yawRad),
            std::sin(pitchRad),
            std::cos(pitchRad) * std::cos(yawRad));
        glm::vec3 eye = res.camera.target + dir * res.camera.distance;
        res.cameraEye = eye;

        glm::mat4 view = glm::lookAt(eye, res.camera.target, glm::vec3(0.0f, 1.0f, 0.0f));

        float aspect = static_cast<float>(ctx.swapchain_extent.width) /
                       static_cast<float>(ctx.swapchain_extent.height);
        glm::mat4 proj;
        if (res.camera.usePerspective)
        {
            proj = glm::perspective(glm::radians(res.camera.fovDeg), aspect, 0.1f, 100.0f);
        }
        else
        {
            float h = res.camera.orthoHalfHeight;
            float w = h * aspect;
            proj = glm::ortho(-w, w, -h, h, 0.1f, 100.0f);
        }

        const glm::mat4 vulkanCorrection(
            glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, 0.5f, 0.0f),
            glm::vec4(0.0f, 0.0f, 0.5f, 1.0f));
        proj = vulkanCorrection * proj;

        uint32_t pc = std::min<uint32_t>((uint32_t)res.pointLights.size(), Resources::MAX_LIGHTS);
        uint32_t sc = std::min<uint32_t>((uint32_t)res.spotLights.size(), Resources::MAX_LIGHTS);

        SceneUBO scene{};
        scene.view = view;
        scene.proj = proj;
        scene.cameraPos = glm::vec4(eye, 1.0f);
        scene.ambient = glm::vec4(res.ambientColor, res.ambientIntensity);
        scene.dirDirection = glm::vec4(glm::normalize(res.dirLightDirection), 0.0f);
        scene.dirColor = glm::vec4(res.dirLightColor, res.dirLightIntensity);
        scene.lightCounts = glm::vec4((float)pc, (float)sc, 0.0f, 0.0f);
        std::memcpy(res.sceneUBOMapped, &scene, sizeof(scene));

        auto *pOut = static_cast<PointLight *>(res.pointSSBOMapped);
        for (uint32_t i = 0; i < pc; ++i)
        {
            const auto &l = res.pointLights[i];
            pOut[i].position = glm::vec4(l.position, l.intensity);
            pOut[i].color = glm::vec4(l.color, 1.0f);
            pOut[i].params = glm::vec4(l.constant, l.linear, l.quadratic, 0.0f);
        }
        for (uint32_t i = pc; i < Resources::MAX_LIGHTS; ++i)
        {
            pOut[i].position = glm::vec4(0.0f);
            pOut[i].color = glm::vec4(0.0f);
            pOut[i].params = glm::vec4(0.0f);
        }

        auto *sOut = static_cast<SpotLight *>(res.spotSSBOMapped);
        for (uint32_t i = 0; i < sc; ++i)
        {
            const auto &l = res.spotLights[i];
            sOut[i].position = glm::vec4(l.position, l.intensity);
            sOut[i].direction = glm::vec4(glm::normalize(l.direction), 0.0f);
            sOut[i].color = glm::vec4(l.color, 1.0f);

            float ci = std::cos(glm::radians(l.innerAngleDeg));
            float co = std::cos(glm::radians(l.outerAngleDeg));
            if (co > ci)
                std::swap(ci, co);
            sOut[i].angles = glm::vec4(ci, co, 0.0f, 0.0f);
            sOut[i].params = glm::vec4(l.constant, l.linear, l.quadratic, 0.0f);
        }
        for (uint32_t i = sc; i < Resources::MAX_LIGHTS; ++i)
        {
            sOut[i].position = glm::vec4(0.0f);
            sOut[i].direction = glm::vec4(0.0f);
            sOut[i].color = glm::vec4(0.0f);
            sOut[i].angles = glm::vec4(0.0f);
            sOut[i].params = glm::vec4(0.0f);
        }

        for (int i = 0; i < res.objectCount; ++i)
        {
            auto &obj = res.objects[i];
            ModelUBO mubo{};

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, obj.position);
            if (obj.animating || obj.animTime > 0.0)
            {
                float t = static_cast<float>(obj.animTime);
                float blend = std::min(t, 1.0f);
                model = glm::translate(model,
                                       glm::vec3(std::cos(t) * obj.pathRadius * blend, 0.0f,
                                                 std::sin(t) * obj.pathRadius * blend));
            }
            model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1, 0, 0));
            model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0, 1, 0));
            model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0, 0, 1));
            if (obj.animating || obj.animTime > 0.0)
                model = glm::rotate(model, static_cast<float>(obj.animTime), glm::vec3(0, 1, 0));
            model = glm::scale(model, obj.scale);
            obj.lastModel = model;

            mubo.model = model;
            mubo.normalMatrix = glm::transpose(glm::inverse(model));
            mubo.albedo = glm::vec4(obj.albedo, obj.alpha);
            mubo.specularColor = glm::vec4(obj.specularColor, obj.shininess);
            std::memcpy(obj.uniformMapped, &mubo, sizeof(mubo));
        }

        res.coneVertexWritten = 0;
        if (res.showLightGizmos && !res.spotLights.empty() && res.coneVertexMapped)
        {
            auto *coneOut = static_cast<Vertex *>(res.coneVertexMapped);
            for (const auto &l : res.spotLights)
            {
                auto lines = makeSpotConeLines(
                    l.position, l.direction,
                    glm::radians(l.outerAngleDeg),
                    2.5f, 16);
                if (res.coneVertexWritten + (uint32_t)lines.size() > res.coneVertexCapacity)
                    break;
                std::memcpy(coneOut + res.coneVertexWritten,
                            lines.data(), lines.size() * sizeof(Vertex));
                res.coneVertexWritten += (uint32_t)lines.size();
            }
        }
    }


    void record(VkCommandBuffer cmd, uint32_t imageIndex)
    {
        (void)imageIndex;
        auto &ctx = graphics::internal::context;

        VkViewport vp{0.0f, 0.0f,
                      static_cast<float>(ctx.swapchain_extent.width),
                      static_cast<float>(ctx.swapchain_extent.height),
                      0.0f, 1.0f};
        VkRect2D sc{{0, 0}, ctx.swapchain_extent};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        VkDeviceSize offsets[] = {0};

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.pipelineLayout,
                                0, 1, &res.sceneSet, 0, nullptr);
        vkCmdBindVertexBuffers(cmd, 0, 1, &res.vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmd, res.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        std::vector<int> opaqueIdx, transparentIdx;
        for (int i = 0; i < res.objectCount; ++i)
        {
            if (res.objects[i].alpha >= 0.999f)
                opaqueIdx.push_back(i);
            else
                transparentIdx.push_back(i);
        }
        std::sort(transparentIdx.begin(), transparentIdx.end(), [&](int a, int b)
                  {
        glm::vec3 pa = glm::vec3(res.objects[a].lastModel * glm::vec4(0,0,0,1));
        glm::vec3 pb = glm::vec3(res.objects[b].lastModel * glm::vec4(0,0,0,1));
        return glm::dot(pa - res.cameraEye, pa - res.cameraEye)
             > glm::dot(pb - res.cameraEye, pb - res.cameraEye); });

        auto drawOne = [&](int i)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.pipelineLayout,
                                    1, 1, &res.objects[i].descriptorSet, 0, nullptr);
            vkCmdDrawIndexed(cmd, res.indexCount, 1, 0, 0, 0);
        };

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.pipeline);
        for (int i : opaqueIdx)
            drawOne(i);

        if (!transparentIdx.empty())
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.transparentPipeline);
            for (int i : transparentIdx)
                drawOne(i);
        }

        if (!res.showLightGizmos)
            return;

        struct PushData
        {
            glm::mat4 model;
            glm::vec4 color;
        } push{};

        if (!res.pointLights.empty())
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.visTriPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.visPipelineLayout,
                                    0, 1, &res.sceneSet, 0, nullptr);
            vkCmdBindVertexBuffers(cmd, 0, 1, &res.cubeVertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, res.cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

            for (const auto &l : res.pointLights)
            {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), l.position);
                model = glm::scale(model, glm::vec3(0.1f));
                push.model = model;
                push.color = glm::vec4(l.color, 1.0f);
                vkCmdPushConstants(cmd, res.visPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                   0, sizeof(push), &push);
                vkCmdDrawIndexed(cmd, res.cubeIndexCount, 1, 0, 0, 0);
            }
        }

        if (!res.spotLights.empty())
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.visTriPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.visPipelineLayout,
                                    0, 1, &res.sceneSet, 0, nullptr);
            vkCmdBindVertexBuffers(cmd, 0, 1, &res.cubeVertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, res.cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

            for (const auto &l : res.spotLights)
            {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), l.position);
                model = glm::scale(model, glm::vec3(0.1f));
                push.model = model;
                push.color = glm::vec4(l.color, 1.0f);
                vkCmdPushConstants(cmd, res.visPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                   0, sizeof(push), &push);
                vkCmdDrawIndexed(cmd, res.cubeIndexCount, 1, 0, 0, 0);
            }
        }

        if (res.coneVertexWritten > 0)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.visLinePipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res.visPipelineLayout,
                                    0, 1, &res.sceneSet, 0, nullptr);

            push.model = glm::mat4(1.0f);
            push.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            vkCmdPushConstants(cmd, res.visPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(push), &push);

            vkCmdBindVertexBuffers(cmd, 0, 1, &res.coneVertexBuffer, offsets);
            vkCmdDraw(cmd, res.coneVertexWritten, 1, 0, 0);
        }
    }


    void shutdown()
    {
        auto &ctx = graphics::internal::context;
        vkDeviceWaitIdle(ctx.device);

        if (res.pipeline)
            vkDestroyPipeline(ctx.device, res.pipeline, nullptr);
        if (res.transparentPipeline)
            vkDestroyPipeline(ctx.device, res.transparentPipeline, nullptr);
        if (res.visTriPipeline)
            vkDestroyPipeline(ctx.device, res.visTriPipeline, nullptr);
        if (res.visLinePipeline)
            vkDestroyPipeline(ctx.device, res.visLinePipeline, nullptr);
        if (res.pipelineLayout)
            vkDestroyPipelineLayout(ctx.device, res.pipelineLayout, nullptr);
        if (res.visPipelineLayout)
            vkDestroyPipelineLayout(ctx.device, res.visPipelineLayout, nullptr);
        if (res.scenePool)
            vkDestroyDescriptorPool(ctx.device, res.scenePool, nullptr);
        if (res.modelPool)
            vkDestroyDescriptorPool(ctx.device, res.modelPool, nullptr);
        if (res.sceneSetLayout)
            vkDestroyDescriptorSetLayout(ctx.device, res.sceneSetLayout, nullptr);
        if (res.modelSetLayout)
            vkDestroyDescriptorSetLayout(ctx.device, res.modelSetLayout, nullptr);

        if (res.sceneUBO)
            vmaDestroyBuffer(ctx.allocator, res.sceneUBO, res.sceneUBOAlloc);
        if (res.pointSSBO)
            vmaDestroyBuffer(ctx.allocator, res.pointSSBO, res.pointSSBOAlloc);
        if (res.spotSSBO)
            vmaDestroyBuffer(ctx.allocator, res.spotSSBO, res.spotSSBOAlloc);

        for (int i = 0; i < Resources::MAX_OBJECTS; ++i)
        {
            auto &obj = res.objects[i];
            if (obj.uniformBuffer)
                vmaDestroyBuffer(ctx.allocator, obj.uniformBuffer, obj.uniformAlloc);
        }

        if (res.vertexBuffer)
            vmaDestroyBuffer(ctx.allocator, res.vertexBuffer, res.vertexAlloc);
        if (res.indexBuffer)
            vmaDestroyBuffer(ctx.allocator, res.indexBuffer, res.indexAlloc);
        if (res.cubeVertexBuffer)
            vmaDestroyBuffer(ctx.allocator, res.cubeVertexBuffer, res.cubeVertexAlloc);
        if (res.cubeIndexBuffer)
            vmaDestroyBuffer(ctx.allocator, res.cubeIndexBuffer, res.cubeIndexAlloc);
        if (res.coneVertexBuffer)
            vmaDestroyBuffer(ctx.allocator, res.coneVertexBuffer, res.coneVertexAlloc);
    }

    Resources &params() { return res; }

} // namespace shape_renderer