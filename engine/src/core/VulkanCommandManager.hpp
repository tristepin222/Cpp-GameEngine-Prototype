#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

/**
 * @class VulkanCommandManager
 * @brief Manages the command pool, frame command buffers, and one-time utility command buffer operations.
 */
class VulkanCommandManager {
public:
    /**
     * @brief Construct a new Vulkan Command Manager object.
     */
    VulkanCommandManager() = default;
    /**
     * @brief Destroy the Vulkan Command Manager object and release command pool.
     */
    ~VulkanCommandManager();

    VulkanCommandManager(const VulkanCommandManager&) = delete;
    VulkanCommandManager& operator=(const VulkanCommandManager&) = delete;

    /**
     * @brief Creates command pool and command buffers.
     * @param device Vulkan logical device context.
     * @param queueFamilyIndex Index of the target hardware queue family.
     * @param commandBufferCount Number of primary frame buffers to allocate.
     */
    void create(VkDevice device, uint32_t queueFamilyIndex, uint32_t commandBufferCount = 2);
    /**
     * @brief Safely destroys the command pool and associated command buffers.
     */
    void destroy();

    /**
     * @brief Gets raw Vulkan command pool.
     * @return VkCommandPool handle.
     */
    VkCommandPool getCommandPool() const { return commandPool; }
    /**
     * @brief Gets command buffer corresponding to current frame index.
     * @return VkCommandBuffer handle.
     */
    VkCommandBuffer getCurrentCommandBuffer() const { return commandBuffers[currentFrame]; }
    /**
     * @brief Gets command buffer at specified array index.
     * @param index Array index.
     * @return VkCommandBuffer handle.
     */
    VkCommandBuffer getCommandBuffer(size_t index) const { return commandBuffers.at(index); }
    /**
     * @brief Gets active double buffering frame index.
     * @return Frame index.
     */
    size_t getCurrentFrame() const { return currentFrame; }

    /**
     * @brief Alternates the frame index and begins recording the next frame command buffer.
     */
    void beginFrame();
    /**
     * @brief Ends command buffer recording for the active frame.
     */
    void endFrame();

    /**
     * @brief Allocates and begins recording a one-time utility command buffer.
     * @return Allocated VkCommandBuffer handle.
     */
    VkCommandBuffer beginOneTimeCommand();
    /**
     * @brief Ends, submits, waits on, and releases a one-time utility command buffer.
     * @param commandBuffer The temporary command buffer.
     * @param queue Queue to submit commands to.
     */
    void endOneTimeCommand(VkCommandBuffer commandBuffer, VkQueue queue);

private:
    /** @brief Reference to Vulkan logical device. */
    VkDevice device = VK_NULL_HANDLE;
    /** @brief Raw command pool handle. */
    VkCommandPool commandPool = VK_NULL_HANDLE;
    /** @brief Array of primary frame command buffers. */
    std::vector<VkCommandBuffer> commandBuffers;
    /** @brief Active frame command buffer array index. */
    size_t currentFrame = 0;
};
