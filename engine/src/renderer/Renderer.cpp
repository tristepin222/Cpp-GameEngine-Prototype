#include "Renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../include/ecs/uniforms/instanceData.hpp"
#include "../include/ecs/uniforms/uniforms.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>

/**
 * @brief Construct a new Renderer:: Renderer object.
 * @param dev Logical Vulkan device.
 * @param extent Viewport layout extent.
 * @param frames Maximum frames allowed in flight.
 */
Renderer::Renderer(VkDevice dev, VkExtent2D extent, uint32_t frames)
    : device(dev), swapchainExtent(extent), framesInFlight(frames) {
    cameraBuffers.resize(frames);
    instanceBuffers.resize(frames);
    descriptorSets.resize(frames);
}

/**
 * @brief Destroy the Renderer:: Renderer object.
 */
Renderer::~Renderer() {
    if (descriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

// --- Update per-frame camera UBO ---
/**
 * @brief Maps Camera Uniform Buffer memory and updates view/proj.
 * @param frameIndex active frame index.
 * @param ubo Camera view projection parameters.
 */
void Renderer::updateCameraUBO(uint32_t frameIndex, const CameraUBO& ubo) {
    VulkanBuffer& buffer = cameraBuffers[frameIndex];
    void* data;
    vkMapMemory(device, buffer.memory, 0, sizeof(CameraUBO), 0, &data);
    memcpy(data, &ubo, sizeof(CameraUBO));
    vkUnmapMemory(device, buffer.memory);
}

// --- Update per-frame instance buffer ---
/**
 * @brief Maps GPU instanced matrices buffer and writes active CPU instance list.
 * @param frameIndex active frame index.
 */
void Renderer::updateInstanceBuffer(uint32_t frameIndex) {
    VulkanBuffer& buffer = instanceBuffers[frameIndex];
    // Copy current transforms into instanceDataCPU
    instanceDataCPU.resize(transforms.size());
    for (size_t i = 0; i < transforms.size(); ++i)
        instanceDataCPU[i].model = transforms[i].getMatrix();

    void* data;
    vkMapMemory(device, buffer.memory, 0, sizeof(InstanceData) * instanceDataCPU.size(), 0, &data);
    memcpy(data, instanceDataCPU.data(), sizeof(InstanceData) * instanceDataCPU.size());
    vkUnmapMemory(device, buffer.memory);
}

// --- Batch building ---
/**
 * @brief Collects active renderables and builds rendering batches.
 */
void Renderer::buildBatches() {
    batches.clear();
    std::unordered_map<uint64_t, RenderBatch> batchMap;
    for (uint32_t i = 0; i < transforms.size(); ++i) {
        auto& r = renderables[i];
        uint64_t key = (uint64_t(r.meshID) << 32) | r.materialID;
        auto& batch = batchMap[key];
        batch.meshID = r.meshID;
        batch.materialID = r.materialID;
        batch.entityIndices.push_back(i);
    }
    for (auto& [_, batch] : batchMap)
        batches.push_back(batch);
}

// --- Render a frame using per-frame buffers ---
/**
 * @brief Begins render passes and records draws for active batches.
 * @param cmdBuffer Vulkan command buffer handle.
 * @param pipeline Compiled graphics pipeline.
 * @param layout pipeline layout layout.
 * @param framebuffer target framebuffer.
 * @param renderPass active render pass.
 * @param extent Target rendering resolution.
 * @param frameIndex active frame index.
 */
void Renderer::renderFrame(VkCommandBuffer cmdBuffer, VkPipeline pipeline,
    VkPipelineLayout layout,
    VkFramebuffer framebuffer,
    VkRenderPass renderPass,
    VkExtent2D extent,
    uint32_t frameIndex) {
    buildBatches();
    updateInstanceBuffer(frameIndex);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = framebuffer;
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = extent;

    VkClearValue clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmdBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind per-frame descriptor set
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
        &descriptorSets[frameIndex], 0, nullptr);

    for (auto& batch : batches) {
        auto& mesh = meshTable[batch.meshID];

        VkBuffer vertexBuffers[] = { mesh.vertexBuffer, instanceBuffers[frameIndex].get() };
        VkDeviceSize offsets[] = { 0, 0 };

        vkCmdBindVertexBuffers(cmdBuffer, 0, 2, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdDrawIndexed(cmdBuffer, mesh.indexCount,
            static_cast<uint32_t>(batch.entityIndices.size()), 0, 0, 0);
    }

    vkCmdEndRenderPass(cmdBuffer);
}
