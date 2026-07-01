#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
/**
 * @struct Material
 * @brief Represents a material component defining rendering properties.
 */
struct Material {

    /** @brief Unique identifier for this material. */
    uint32_t id;

    /** @brief The base color of the material (RGBA). */
    glm::vec4 color{ 1.0f }; // RGBA
    /** @brief Vulkan descriptor set representing resource bindings. */
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    /** @brief Optional per-material custom Vulkan pipeline. */
    VkPipeline pipeline{ VK_NULL_HANDLE }; // optional per-shader pipeline
    /** @brief Optional per-material custom Vulkan pipeline layout. */
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE }; // optional per-shader pipeline layout

    /**
     * @brief Construct a new Material object.
     * @param c Color of the material.
     * @param ds Descriptor set associated with this material.
     * @param pp Custom graphics pipeline.
     */
    Material(const glm::vec4& c = { 1.f,1.f,1.f,1.f }, VkDescriptorSet ds = VK_NULL_HANDLE, VkPipeline pp = VK_NULL_HANDLE)
        : color(c), descriptorSet(ds) {
    }
};
