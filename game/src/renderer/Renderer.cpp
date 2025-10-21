#include "Renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../include/ecs/uniforms/instanceData.hpp"
#include "../include/ecs/uniforms/uniforms.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <array>
#include <fstream>
#include <algorithm>

Renderer::Renderer(VkDevice device, VkExtent2D extent, uint32_t framesInFlight, MeshManager& meshManager)
    : device(device), swapchainExtent(extent), framesInFlight(framesInFlight), meshManager(meshManager)
{
    cameraBuffers.reserve(framesInFlight);
    instanceBuffers.reserve(framesInFlight);
    descriptorSets.resize(framesInFlight);
}

Renderer::~Renderer() {
    if (descriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

void Renderer::allocateBuffers(VkPhysicalDevice physicalDevice, size_t maxInstances)
{
    cameraBuffers.clear();
    instanceBuffers.clear();

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        cameraBuffers.emplace_back(
            VulkanBuffer::createUniformBuffer(device, physicalDevice, sizeof(CameraUBO))
        );

        VkBufferUsageFlags instanceUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        instanceBuffers.emplace_back(
            VulkanBuffer::createBuffer(device, physicalDevice, sizeof(InstanceData) * maxInstances, instanceUsage)
        );
    }

    // CPU-side pre-allocations for DOD usage:
    instanceDataCPU.clear();
    instanceDataCPU.reserve(maxInstances);

    batchEntityIndices.clear();
    batchEntityIndices.reserve(maxInstances);

    batches.clear();
    sortKeys.clear();
    sortKeys.reserve(maxInstances);

    // keep an internal maxInstances if you need it later
    this->maxInstances = maxInstances;
}


// --- Update per-frame camera UBO ---
void Renderer::updateCameraUBO(uint32_t frameIndex, const CameraUBO& ubo, const VulkanContext& context) {
    // --- 1️⃣ Bounds check ---
    if (frameIndex >= cameraBuffers.size()) {
        throw std::out_of_range("updateCameraUBO: frameIndex out of range");
    }

    VulkanBuffer& buffer = cameraBuffers[frameIndex];

    // --- 2️⃣ Align size to minUniformBufferOffsetAlignment ---
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(context.physicalDevice, &props);
    VkDeviceSize alignment = props.limits.minUniformBufferOffsetAlignment;
    VkDeviceSize uboSize = sizeof(CameraUBO);
    VkDeviceSize alignedSize = (uboSize + alignment - 1) & ~(alignment - 1);

    // --- 3️⃣ Map memory ---
    void* data = nullptr;
    VkResult r = vkMapMemory(device, buffer.getMemory(), 0, alignedSize, 0, &data);
    if (r != VK_SUCCESS || data == nullptr) {
        throw std::runtime_error("updateCameraUBO: failed to map buffer memory!");
    }

    // --- 4️⃣ Copy data ---
    std::memcpy(data, &ubo, sizeof(CameraUBO));

    // --- 5️⃣ Unmap memory ---
    vkUnmapMemory(device, buffer.getMemory());

}

// --- Update per-frame instance buffer ---
void Renderer::updateInstanceBuffer(uint32_t frameIndex) {
    VulkanBuffer& buffer = instanceBuffers[frameIndex];
    // Copy current transforms into instanceDataCPU
    instanceDataCPU.resize(transforms.size());
    for (size_t i = 0; i < transforms.size(); ++i)
        instanceDataCPU[i].model = transforms[i].getMatrix();

    void* data;
    vkMapMemory(device, buffer.getMemory(), 0, sizeof(InstanceData) * instanceDataCPU.size(), 0, &data);
    memcpy(data, instanceDataCPU.data(), sizeof(InstanceData) * instanceDataCPU.size());
    vkUnmapMemory(device, buffer.getMemory());
}

// --- Batch building ---
void Renderer::buildBatches()
{
    batches.clear();
    batchEntityIndices.clear();
    sortKeys.clear();

    size_t n = transforms.size();
    if (n == 0) return;

    // create key list (key = (meshID<<32) | materialID)
    sortKeys.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& r = renderables[i];
        uint64_t key = (uint64_t(r.meshID) << 32) | uint64_t(r.materialID);
        sortKeys[i] = { key, static_cast<uint32_t>(i) };
    }

    // sort by key so identical mesh/material are contiguous
    std::sort(sortKeys.begin(), sortKeys.end(),
        [](auto& a, auto& b) { return a.first < b.first; });

    // scan sorted keys and create batches; append entity indices into flat array
    size_t idx = 0;
    while (idx < sortKeys.size()) {
        uint64_t key = sortKeys[idx].first;
        uint32_t firstEntity = sortKeys[idx].second;

        uint32_t meshID = uint32_t(key >> 32);
        uint32_t materialID = uint32_t(key & 0xffffffffu);

        RenderBatch batch{};
        batch.meshID = meshID;
        batch.materialID = materialID;
        batch.firstIndex = batchEntityIndices.size();

        // consume runs of same key
        size_t runStart = idx;
        while (idx < sortKeys.size() && sortKeys[idx].first == key) {
            batchEntityIndices.push_back(sortKeys[idx].second);
            ++idx;
        }
        batch.count = batchEntityIndices.size() - batch.firstIndex;

        batches.push_back(batch);
    }
}

// --- Render a frame using per-frame buffers ---
void Renderer::renderFrame(
    VkCommandBuffer cmdBuffer,
    VkFramebuffer framebuffer,
    VkRenderPass renderPass,
    VkExtent2D extent,
    uint32_t frameIndex
) {
    // Update per-frame instance buffer
    buildBatches();
    updateInstanceBuffer(frameIndex);

    PushConstants push{};
    push.model = glm::mat4(1.0f); // or per-instance if needed
    push.color = glm::vec4(1.0f);

    vkCmdPushConstants(
        cmdBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &push
    );


    // --- Begin render pass ---
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = framebuffer;
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = extent;

    VkClearValue clearColor = { {{0.0f, 0.0f, 1.0f, 1.0f}} }; // Blue background
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmdBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // --- Set viewport & scissor ---
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);


    // bind pipeline once if it won't change per-batch — (you bind per batch if pipeline differs)
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    // bind per-frame descriptor set once (camera + instance SSBO bound to set 0)
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
        0, 1, &descriptorSets[frameIndex], 0, nullptr);

    // --- Bind shared buffers once ---
    const VkBuffer vertexBuffers[] = { meshManager.globalVertexBuffer.get()};
    VkDeviceSize vertexOffsets[] = { 0 };
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, vertexOffsets);
    vkCmdBindIndexBuffer(cmdBuffer, meshManager.globalIndexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);

    for (const auto& batch : batches) {
        const Mesh& mesh = meshManager.getMesh(batch.meshID);
        vkCmdDrawIndexed(
            cmdBuffer,
            mesh.indexCount,
            batch.count,
            mesh.indexOffset,
            mesh.vertexOffset,
            batch.firstIndex
        );
    }


    vkCmdEndRenderPass(cmdBuffer);
}

// In Renderer.cpp
void Renderer::initDescriptorSets(VkDescriptorPool pool, size_t maxInstances) {

    // --- 1️⃣ Descriptor Set Layout ---
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout!");


    descriptorSets.resize(framesInFlight); // ensure space for allocation
    // --- 2️⃣ Allocate Descriptor Sets ---
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets!");

    // --- 3️⃣ Update each per-frame descriptor set ---
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = cameraBuffers[i].get();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = sizeof(CameraUBO);

        VkDescriptorBufferInfo instanceBufferInfo{};
        instanceBufferInfo.buffer = instanceBuffers[i].get();
        instanceBufferInfo.offset = 0;
        instanceBufferInfo.range = sizeof(InstanceData) * maxInstances; // use maxInstances

        std::array<VkWriteDescriptorSet, 2> writes{};

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &cameraBufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &instanceBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    std::cout << "Renderer: Descriptor sets initialized successfully for maxInstances=" << maxInstances << std::endl;
}


void Renderer::bindPipelineAndDescriptors(VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
        &descriptorSets[frameIndex], 0, nullptr);
}

void Renderer::setupPipeline(VkDevice device, VkRenderPass renderPass) {
    PipelineBuilder builder;

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants); // match your shader struct size

    // --- Pipeline layout ---
    pipelineLayout = builder.createPipelineLayout(device, descriptorSetLayout, &pushConstant, 1);
    builder.setLayout(pipelineLayout);
    builder.setRenderPass(renderPass);


    // --- Load shaders ---
    auto loadShader = [&](const std::string& path) -> VkShaderModule {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("Failed to open shader: " + path);

        size_t size = file.tellg();
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read(buffer.data(), size);
        file.close();

        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = buffer.size();
        info.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

        VkShaderModule shader;
        if (vkCreateShaderModule(device, &info, nullptr, &shader) != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module: " + path);
        return shader;
        };

    VkShaderModule vertShader = loadShader("build/shaders/unlit.vert.spv");
    VkShaderModule fragShader = loadShader("build/shaders/unlit.frag.spv");
    builder.setShaders(vertShader, fragShader);

    // --- Vertex input ---
    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(Vertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription instanceBinding{};
    instanceBinding.binding = 1;
    instanceBinding.stride = sizeof(InstanceData);
    instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::vector<VkVertexInputAttributeDescription> attributes = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::vec4) * 0},
        {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::vec4) * 1},
        {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::vec4) * 2},
        {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::vec4) * 3},
    };

    builder.setVertexInput({ vertexBinding, instanceBinding }, attributes);

    // --- Build pipeline ---
    auto [pipe, layout] = builder.build(device);
    pipeline = pipe;
    pipelineLayout = layout;

    // --- Cleanup shader modules ---
    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}
void Renderer::cleanupSwapchainResources() {
    // Destroy pipeline & layout if created
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    // If the renderer created a render pass or framebuffers (not shown in your snippets),
    // destroy them here. If your SwapchainManager owns renderPass/framebuffers, skip these.
    // e.g.
    // if (framebuffersCreatedByRenderer) {
    //     for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    //     framebuffers.clear();
    // }
}
