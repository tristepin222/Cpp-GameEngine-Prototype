#pragma once
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>

/**
 * @class PipelineBuilder
 * @brief Builder utility facilitating the creation of Vulkan graphics pipelines.
 */
class PipelineBuilder {
public:
    /**
     * @brief Construct a new Pipeline Builder object.
     */
    PipelineBuilder() = default;
    /**
     * @brief Destroy the Pipeline Builder object.
     */
    ~PipelineBuilder() = default;

    // Basic setters
    /**
     * @brief Configures the pipeline layout.
     * @param l Pipeline layout handle.
     * @return Reference to self for chaining.
     */
    PipelineBuilder& setLayout(VkPipelineLayout l) { layout = l; return *this; }
    /**
     * @brief Configures the target render pass.
     * @param rp Render pass handle.
     * @return Reference to self for chaining.
     */
    PipelineBuilder& setRenderPass(VkRenderPass rp) { renderPass = rp; return *this; }
    /**
     * @brief Configures the shader stages.
     * @param vs Vertex shader module.
     * @param fs Fragment shader module.
     * @return Reference to self for chaining.
     */
    PipelineBuilder& setShaders(VkShaderModule vs, VkShaderModule fs) { vertShader = vs; fragShader = fs; return *this; }

    // Provide full vertex input descriptions (overwrites previously set bindings/attributes)
    /**
     * @brief Configures vertex input binding and attribute descriptions.
     * @param bindings Layout binding list.
     * @param attributes Layout attribute list.
     * @return Reference to self for chaining.
     */
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
    /**
     * @brief Adds input descriptions mapping to an instance model matrix.
     * @param binding Target vertex buffer binding index.
     * @param locationStart Starting shader location.
     * @return Reference to self for chaining.
     */
    PipelineBuilder& addInstanceMatrixBinding(uint32_t binding, uint32_t locationStart);

    // Build pipeline
    /**
     * @brief Compiles and constructs the graphics pipeline.
     * @param device Vulkan logical device context.
     * @return VkPipeline handle.
     */
    VkPipeline build(VkDevice device);

private:
    /** @brief Reference to pipeline layout. */
    VkPipelineLayout layout = VK_NULL_HANDLE;
    /** @brief Target render pass index. */
    VkRenderPass renderPass = VK_NULL_HANDLE;
    /** @brief Vertex shader handle. */
    VkShaderModule vertShader = VK_NULL_HANDLE;
    /** @brief Fragment shader handle. */
    VkShaderModule fragShader = VK_NULL_HANDLE;

    /** @brief Cached input bindings. */
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    /** @brief Cached input attributes. */
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
};
