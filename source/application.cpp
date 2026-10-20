#include "application.hpp"
#include "shape_renderer.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <cstdio>

namespace application {

bool initialize() {
    return shape_renderer::initialize();
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

    ImGui::Begin("Camera");
    ImGui::SliderFloat("Yaw", &p.camera.yaw, -180.0f, 180.0f);
    ImGui::SliderFloat("Pitch", &p.camera.pitch, -89.0f,  89.0f);
    ImGui::SliderFloat("Distance", &p.camera.distance, 0.5f, 30.0f);
    ImGui::SliderFloat("FOV", &p.camera.fovDeg, 20.0f, 120.0f);
    ImGui::Checkbox("Perspective", &p.camera.usePerspective);
    if (!p.camera.usePerspective) {
        ImGui::SliderFloat("Ortho half-height", &p.camera.orthoHalfHeight, 0.5f, 20.0f);
    }
    ImGui::SliderFloat3("Target",&p.camera.target.x, -10.0f, 10.0f);
    ImGui::End();

    ImGui::Begin("Scene Light");
    ImGui::Text("Ambient");
    ImGui::ColorEdit3("Ambient color", &p.ambientColor.x);
    ImGui::SliderFloat("Ambient intensity", &p.ambientIntensity, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Directional light");
    ImGui::SliderFloat3("Dir direction", &p.dirLightDirection.x, -1.0f, 1.0f);
    ImGui::ColorEdit3("Dir color", &p.dirLightColor.x);
    ImGui::SliderFloat("Dir intensity", &p.dirLightIntensity, 0.0f, 5.0f);

    ImGui::Separator();
    ImGui::Checkbox("Show light gizmos", &p.showLightGizmos);
    ImGui::End();

    ImGui::Begin("Point Lights");
    if (ImGui::Button("Add point light")) {
        if (p.pointLights.size() < shape_renderer::Resources::MAX_LIGHTS)
            p.pointLights.push_back({});
    }
    ImGui::SameLine();
    ImGui::Text("Count: %d", (int)p.pointLights.size());

    int removeIdx = -1;
    for (int i = 0; i < (int)p.pointLights.size(); ++i) {
        auto& l = p.pointLights[i];
        ImGui::PushID(i);
        ImGui::Separator();
        ImGui::Text("Light %d", i);
        ImGui::SliderFloat3("Position", &l.position.x, -10.0f, 10.0f);
        ImGui::ColorEdit3("Color", &l.color.x);
        ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 10.0f);
        ImGui::SliderFloat("Constant", &l.constant, 0.0f, 5.0f);
        ImGui::SliderFloat("Linear", &l.linear, 0.0f, 1.0f);
        ImGui::SliderFloat("Quadratic", &l.quadratic, 0.0f, 1.0f);
        if (ImGui::Button("Remove")) removeIdx = i;
        ImGui::PopID();
    }
    if (removeIdx >= 0) p.pointLights.erase(p.pointLights.begin() + removeIdx);
    ImGui::End();

    ImGui::Begin("Spot Lights");
    if (ImGui::Button("Add spot light")) {
        if (p.spotLights.size() < shape_renderer::Resources::MAX_LIGHTS)
            p.spotLights.push_back({});
    }
    ImGui::SameLine();
    ImGui::Text("Count: %d", (int)p.spotLights.size());

    removeIdx = -1;
    for (int i = 0; i < (int)p.spotLights.size(); ++i) {
        auto& l = p.spotLights[i];
        ImGui::PushID(1000 + i);
        ImGui::Separator();
        ImGui::Text("Spot %d", i);
        ImGui::SliderFloat3("Position", &l.position.x, -10.0f, 10.0f);
        ImGui::SliderFloat3("Direction", &l.direction.x, -1.0f, 1.0f);
        ImGui::ColorEdit3("Color", &l.color.x);
        ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 20.0f);
        ImGui::SliderFloat("Inner angle", &l.innerAngleDeg, 0.0f, 89.0f);
        ImGui::SliderFloat("Outer angle", &l.outerAngleDeg, 0.0f, 89.0f);
        if (l.outerAngleDeg < l.innerAngleDeg) l.outerAngleDeg = l.innerAngleDeg;
        ImGui::SliderFloat("Constant",  &l.constant, 0.0f, 5.0f);
        ImGui::SliderFloat("Linear", &l.linear, 0.0f, 1.0f);
        ImGui::SliderFloat("Quadratic", &l.quadratic, 0.0f, 1.0f);
        if (ImGui::Button("Remove")) removeIdx = i;
        ImGui::PopID();
    }
    if (removeIdx >= 0) p.spotLights.erase(p.spotLights.begin() + removeIdx);
    ImGui::End();

    ImGui::Begin("Objects");
    ImGui::SliderInt("Object count", &p.objectCount, 1, shape_renderer::Resources::MAX_OBJECTS);
    for (int i = 0; i < p.objectCount; ++i) {
        if (i > 0) ImGui::SameLine();
        char label[32];
        std::snprintf(label, sizeof(label), "Obj %d", i);
        ImGui::RadioButton(label, &p.selectedObject, i);
    }
    if (p.selectedObject >= p.objectCount) p.selectedObject = 0;
    if (p.selectedObject < 0) p.selectedObject = 0;

    auto& obj = p.objects[p.selectedObject];
    ImGui::Separator();
    ImGui::Text("Editing: Object %d", p.selectedObject);
    ImGui::SliderFloat3("Position", &obj.position.x, -5.0f, 5.0f);
    ImGui::SliderFloat3("Rotation", &obj.rotation.x, -180.0f, 180.0f);
    ImGui::SliderFloat3("Scale", &obj.scale.x, 0.1f, 2.0f);

    ImGui::Separator();
    ImGui::Text("Material (Blinn-Phong)");
    ImGui::ColorEdit3("Albedo", &obj.albedo.x);
    ImGui::SliderFloat("Alpha", &obj.alpha, 0.0f, 1.0f);
    ImGui::ColorEdit3("Specular color", &obj.specularColor.x);
    ImGui::SliderFloat("Shininess", &obj.shininess, 1.0f, 256.0f);

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
    clears[0].color = {{0.02f, 0.02f, 0.03f, 1.0f}};
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

} // namespace application