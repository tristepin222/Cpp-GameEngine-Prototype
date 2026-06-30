#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include "ecs/components/pushconstants.hpp"
#include "ecs/components/Mesh.hpp"

class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    void create(VkDevice device,
        VkExtent2D swapchainExtent,
        VkRenderPass renderPass,
        const std::string& vertPath,
        const std::string& fragPath,
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = {});

    void destroy();

    VkPipeline get() const { return graphicsPipeline; }
    VkPipelineLayout getLayout() const { return pipelineLayout; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

private:
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> loadFile(const std::string& filename);
};
