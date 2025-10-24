#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "../ComponentBase.hpp"

struct Material : ComponentBase {

    uint32_t id;

    glm::vec4 color{ 1.0f }; // RGBA
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    VkPipeline pipeline{ VK_NULL_HANDLE }; // optional per-shader pipeline
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE }; // optional per-shader pipeline layout

    // Constructor
    Material(const glm::vec4& c = { 1.f,1.f,1.f,1.f }, VkDescriptorSet ds = VK_NULL_HANDLE, VkPipeline pp = VK_NULL_HANDLE)
        : color(c), descriptorSet(ds) {
    }
};
