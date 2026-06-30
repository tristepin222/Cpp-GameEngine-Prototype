#pragma once
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>

class PipelineBuilder {
public:
    PipelineBuilder() = default;
    ~PipelineBuilder() = default;

    // Basic setters
    PipelineBuilder& setLayout(VkPipelineLayout l) { layout = l; return *this; }
    PipelineBuilder& setRenderPass(VkRenderPass rp) { renderPass = rp; return *this; }
    PipelineBuilder& setShaders(VkShaderModule vs, VkShaderModule fs) { vertShader = vs; fragShader = fs; return *this; }

    // Provide full vertex input descriptions (overwrites previously set bindings/attributes)
    PipelineBuilder& setVertexInput(
        const std::vector<VkVertexInputBindingDescription>& bindings,
        const std::vector<VkVertexInputAttributeDescription>& attributes)
    {
        vertexBindings = bindings;
        vertexAttributes = attributes;
        return *this;
    }

    // Convenience: add instance matrix binding at `binding` (mat4 -> 4 vec4 attributes)
    // `locationStart` is the first attribute location that will be used (uses locationStart..locationStart+3)
    PipelineBuilder& addInstanceMatrixBinding(uint32_t binding, uint32_t locationStart);

    // Build pipeline
    VkPipeline build(VkDevice device);

private:
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkShaderModule vertShader = VK_NULL_HANDLE;
    VkShaderModule fragShader = VK_NULL_HANDLE;

    // Vertex input descriptions maintained here
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
};
