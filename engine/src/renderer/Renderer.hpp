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

/**
 * @struct RenderBatch
 * @brief Batch of entities sharing the same mesh and material assets.
 */
struct RenderBatch {
    /** @brief Target mesh table index. */
    uint32_t meshID;
    /** @brief Target material table index. */
    uint32_t materialID;
    /** @brief ECS indices of entities included in this batch. */
    std::vector<uint32_t> entityIndices;
};

/**
 * @class Renderer
 * @brief Implements simple batch-rendering pipeline using raw arrays (SoA/DOD style).
 */
class Renderer {
public:
    /**
     * @brief Construct a new Renderer object.
     * @param device Vulkan logical device context.
     * @param extent Viewport extent.
     * @param framesInFlight Max frames in flight.
     */
    Renderer(VkDevice device, VkExtent2D extent, uint32_t framesInFlight);
    /**
     * @brief Destroy the Renderer object and free layouts.
     */
    ~Renderer();

    // ECS/DOD data
    /** @brief Raw array of entity transforms. */
    std::vector<Transform> transforms;
    /** @brief Raw array of renderable components. */
    std::vector<Renderable> renderables;
    /** @brief Loaded meshes lookup table. */
    std::vector<Mesh> meshTable;
    /** @brief Loaded materials lookup table. */
    std::vector<Material> materialTable;

    /**
     * @brief Updates camera uniform buffer descriptors.
     * @param frameIndex Active frame array index.
     * @param ubo Target camera variables.
     */
    void updateCameraUBO(uint32_t frameIndex, const CameraUBO& ubo);
    /**
     * @brief Refreshes active model matrices inside Vulkan buffers.
     * @param frameIndex Active frame array index.
     */
    void updateInstanceBuffer(uint32_t frameIndex);

    /**
     * @brief Rebuilds batch mappings grouped by mesh and material.
     */
    void buildBatches();
    /**
     * @brief records frame rendering pass details into active command buffers.
     * @param cmdBuffer Vulkan command recording handle.
     * @param pipeline Graphics pipeline context.
     * @param layout Pipeline layout context.
     * @param framebuffer Target rendering framebuffer.
     * @param renderPass active render pass.
     * @param extent Target rendering resolution.
     * @param frameIndex Active frame array index.
     */
    void renderFrame(VkCommandBuffer cmdBuffer, VkPipeline pipeline,
        VkPipelineLayout layout,
        VkFramebuffer framebuffer,
        VkRenderPass renderPass,
        VkExtent2D extent,
        uint32_t frameIndex);

    // Descriptor sets
    /** @brief Descriptor layout mapping descriptors. */
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    /** @brief Array of descriptor sets (one for each frame in flight). */
    std::vector<VkDescriptorSet> descriptorSets; // per-frame

private:
    /** @brief Reference to logical device. */
    VkDevice device;
    /** @brief Swapchain display dimensions. */
    VkExtent2D swapchainExtent;
    /** @brief Max double buffering constraints. */
    uint32_t framesInFlight;

    /** @brief Camera uniform buffers. */
    std::vector<VulkanBuffer> cameraBuffers;    // per-frame
    /** @brief Instanced model matrix buffers. */
    std::vector<VulkanBuffer> instanceBuffers;  // per-frame
    /** @brief CPU copy of instance structures. */
    std::vector<InstanceData> instanceDataCPU;  // CPU copy

    /** @brief Active render batch list. */
    std::vector<RenderBatch> batches;

    /**
     * @brief Configures descriptor set allocations.
     * @param pool target descriptor pool.
     */
    void initDescriptorSets(VkDescriptorPool pool);
};
