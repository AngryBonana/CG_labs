#include "application.hpp"
#include "shape_renderer.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <cstdio>

namespace application {

bool initialize() {
    if (!shape_renderer::initialize()) return false;
    return true;
}

void shutdown() {
    shape_renderer::shutdown();
    auto& context = graphics::internal::context;
    vkQueueWaitIdle(context.graphics_queue);
}

void update(double time) {
    static double last = time;
    double dt = time - last;
    last = time;

    shape_renderer::update(dt);

    auto& p = shape_renderer::params();

    ImGui::Begin("Shape Controls");

    ImGui::Checkbox("Perspective projection", &p.usePerspective);
    ImGui::SliderFloat("Camera distance", &p.cameraDistance, 2.0f, 15.0f);

    ImGui::Separator();

    ImGui::SliderInt("Object count", &p.objectCount, 1,
                     shape_renderer::Resources::MAX_OBJECTS);

    ImGui::Text("Select object to edit:");
    for (int i = 0; i < p.objectCount; ++i) {
        if (i > 0) ImGui::SameLine();
        char label[32];
        std::snprintf(label, sizeof(label), "Object %d", i);
        if (ImGui::RadioButton(label, &p.selectedObject, i)) {
        }
    }

    if (p.selectedObject >= p.objectCount) {
        p.selectedObject = 0;
    }
    if (p.selectedObject < 0) {
        p.selectedObject = 0;
    }

    auto& obj = p.objects[p.selectedObject];

    ImGui::Separator();
    ImGui::Text("Editing: Object %d", p.selectedObject);

    ImGui::SliderFloat3("Position", &obj.position.x, -5.0f, 5.0f);
    ImGui::SliderFloat3("Rotation", &obj.rotation.x, -180.0f, 180.0f);
    ImGui::SliderFloat3("Scale", &obj.scale.x, 0.1f, 2.0f);

    ImGui::ColorEdit3("Base color", &obj.baseColor.x);
    ImGui::SliderFloat("Alpha", &obj.alpha, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Animation (Object %d)", p.selectedObject);

    if (ImGui::Button(obj.animating ? "Pause" : "Play")) {
        obj.animating = !obj.animating;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        obj.animTime = 0.0;
        obj.animating = false;
    }

    ImGui::SliderFloat("Anim speed", &obj.animSpeed, 0.1f, 5.0f);
    ImGui::SliderFloat("Path radius", &obj.pathRadius, 0.0f, 3.0f);

    ImGui::End();
}

void render(const graphics::internal::FrameData& fd) {
    auto& ctx = graphics::internal::context;

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(fd.command_buffer, 0);
    vkBeginCommandBuffer(fd.command_buffer, &bi);

    VkClearValue clears[2]{};
    clears[0].color = {{0.05f, 0.05f, 0.08f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = ctx.render_pass;
    rpbi.framebuffer = fd.framebuffer;
    rpbi.renderArea = {{0, 0}, ctx.swapchain_extent};
    rpbi.clearValueCount = 2;
    rpbi.pClearValues = clears;

    vkCmdBeginRenderPass(fd.command_buffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    shape_renderer::record(fd.command_buffer, 0);

    vkCmdEndRenderPass(fd.command_buffer);
    vkEndCommandBuffer(fd.command_buffer);
}

}