#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class VulkanCommandManager {
public:
    VulkanCommandManager() = default;
    ~VulkanCommandManager();

    VulkanCommandManager(const VulkanCommandManager&) = delete;
    VulkanCommandManager& operator=(const VulkanCommandManager&) = delete;

    void create(VkDevice device, uint32_t queueFamilyIndex, uint32_t commandBufferCount = 2);
    void destroy();

    VkCommandPool getCommandPool() const { return commandPool; }
    VkCommandBuffer getCurrentCommandBuffer() const { return commandBuffers[currentFrame]; }
    VkCommandBuffer getCommandBuffer(size_t index) const { return commandBuffers.at(index); }
    size_t getCurrentFrame() const { return currentFrame; }

    void beginFrame();
    void endFrame();

    VkCommandBuffer beginOneTimeCommand();
    void endOneTimeCommand(VkCommandBuffer commandBuffer, VkQueue queue);

private:
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    size_t currentFrame = 0;
};
