#pragma once
#include "../core/Buffer.hpp"
#include "../include/ecs/components/Transform.hpp"
#include "../include/ecs/components/Renderable.hpp"
#include "../include/ecs/components/Mesh.hpp"
#include "../include/ecs/components/Material.hpp"
#include "../include/ecs/uniforms/instanceData.hpp"
#include "../include/ecs/uniforms/uniforms.hpp"
#include "../core/VulkanContext.hpp"
#include "../core/PipelineBuilder.hpp"
#include "../include/ecs/MeshManager.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------
// Renderer — handles GPU buffers, batching, and frame draw
// ---------------------------------------------------------
class Renderer {
public:
    Renderer(VkDevice device, VkExtent2D extent, uint32_t framesInFlight, MeshManager& meshManager);

    ~Renderer();

    // --- ECS/DOD data ---
    std::vector<Transform> transforms;
    std::vector<Renderable> renderables;
    std::vector<Mesh> meshTable;
    std::vector<Material> materialTable;
    std::vector<VulkanBuffer> cameraBuffers;    // per-frame UBOs
    std::vector<VulkanBuffer> instanceBuffers;  // per-frame instance data
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    void setPipeline(VkPipeline pipe, VkPipelineLayout layout) {
		pipeline = pipe;
		pipelineLayout = layout;
	}


    // --- Per-frame updates ---
    void updateCameraUBO(uint32_t frameIndex, const CameraUBO& ubo, const VulkanContext& context);
    void updateInstanceBuffer(uint32_t frameIndex);
    // In Renderer.hpp, public section:
    void initDescriptorSets(VkDescriptorPool pool, size_t maxInstances);
    void allocateBuffers(VkPhysicalDevice physicalDevice, size_t maxInstances);


    // --- Batching & rendering ---
    void buildBatches();
    void renderFrame(
        VkCommandBuffer cmdBuffer,
        VkFramebuffer framebuffer,
        VkRenderPass renderPass,
        VkExtent2D extent,
        uint32_t frameIndex
    );

    void setupPipeline(VkDevice device, VkRenderPass renderPass);
    // --- Descriptor sets ---
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets; // per-frame

    // Bind pipeline + per-frame descriptor set + instance buffer
    void bindPipelineAndDescriptors(
        VkCommandBuffer cmdBuffer,
        uint32_t frameIndex
    );

    // Draw a single render batch
    void cleanupSwapchainResources();

private:
    // --- Core Vulkan state ---
    MeshManager& meshManager;
    VkDevice device;
    VkExtent2D swapchainExtent;
    uint32_t framesInFlight;

    // Renderer class members
    std::vector<InstanceData> instanceDataCPU;        // preallocated CPU-side instance data (SoA-ish)
    std::vector<uint32_t> batchEntityIndices;         // flat list of entity indices for all batches
    std::vector<RenderBatch> batches;                 // linear batches

    size_t maxInstances;
    // optional temporary used during buildBatches:
    std::vector<std::pair<uint64_t, uint32_t>> sortKeys; // pair<key, entityIndex>
};
