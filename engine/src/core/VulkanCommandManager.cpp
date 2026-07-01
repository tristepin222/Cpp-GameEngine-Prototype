#include "VulkanCommandManager.hpp"

/**
 * @brief Destroy the Vulkan Command Manager:: Vulkan Command Manager object.
 */
VulkanCommandManager::~VulkanCommandManager() {
    destroy();
}

/**
 * @brief Destroys the Vulkan command pool.
 */
void VulkanCommandManager::destroy() {
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
}

/**
 * @brief Instantiates the command pool and primary command buffers.
 * @param dev Logical device context.
 * @param queueFamilyIndex Graphics queue family location index.
 * @param commandBufferCount Number of primary command buffers.
 */
void VulkanCommandManager::create(VkDevice dev, uint32_t queueFamilyIndex, uint32_t commandBufferCount) {
    device = dev;

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");

    // Allocate command buffers
    commandBuffers.resize(commandBufferCount);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = commandBufferCount;

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

/**
 * @brief Steps frame index and begins recording next frame command buffer.
 */
void VulkanCommandManager::beginFrame() {
    // For a double-buffered setup, alternate between 0 and 1
    currentFrame = (currentFrame + 1) % commandBuffers.size();

    VkCommandBuffer cmd = commandBuffers[currentFrame];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer");
}

/**
 * @brief Ends frame command buffer recording.
 */
void VulkanCommandManager::endFrame() {
    VkCommandBuffer cmd = commandBuffers[currentFrame];
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("Failed to end command buffer");
}

/**
 * @brief Allocates and starts recording a one-time utility command buffer.
 * @return Allocated command buffer.
 */
VkCommandBuffer VulkanCommandManager::beginOneTimeCommand() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate one-time command buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

/**
 * @brief Submits, waits on, and frees a one-time utility command buffer.
 * @param commandBuffer Temporary command buffer handle.
 * @param queue Command target queue.
 */
void VulkanCommandManager::endOneTimeCommand(VkCommandBuffer commandBuffer, VkQueue queue) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
