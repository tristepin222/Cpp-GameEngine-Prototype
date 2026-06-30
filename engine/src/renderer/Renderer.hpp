#pragma once
#include "../core/Buffer.hpp"
#include "../include/ecs/components/Transform.hpp"
#include "../include/ecs/components/Renderable.hpp"
#include "../include/ecs/components/Mesh.hpp"
#include "../include/ecs/components/Material.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

struct RenderBatch {
    uint32_t meshID;
    uint32_t materialID;
    std::vector<uint32_t> entityIndices;
};

class Renderer {
public:
    Renderer(VkDevice device, VkExtent2D extent, uint32_t framesInFlight);
    ~Renderer();

    // ECS/DOD data
    std::vector<Transform> transforms;
    std::vector<Renderable> renderables;
    std::vector<Mesh> meshTable;
    std::vector<Material> materialTable;

    void updateCameraUBO(uint32_t frameIndex, const CameraUBO& ubo);
    void updateInstanceBuffer(uint32_t frameIndex);

    void buildBatches();
    void renderFrame(VkCommandBuffer cmdBuffer, VkPipeline pipeline,
        VkPipelineLayout layout,
        VkFramebuffer framebuffer,
        VkRenderPass renderPass,
        VkExtent2D extent,
        uint32_t frameIndex);

    // Descriptor sets
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets; // per-frame

private:
    VkDevice device;
    VkExtent2D swapchainExtent;
    uint32_t framesInFlight;

    std::vector<VulkanBuffer> cameraBuffers;    // per-frame
    std::vector<VulkanBuffer> instanceBuffers;  // per-frame
    std::vector<InstanceData> instanceDataCPU;  // CPU copy

    std::vector<RenderBatch> batches;

    void initDescriptorSets(VkDescriptorPool pool);
};
