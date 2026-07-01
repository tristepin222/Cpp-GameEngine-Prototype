#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

/**
 * @class VulkanFrameSync
 * @brief Manages Vulkan semaphores and fences to synchronize frames in flight.
 */
class VulkanFrameSync {
public:
    /**
     * @brief Construct a new Vulkan Frame Sync object.
     */
    VulkanFrameSync() = default;
    /**
     * @brief Destroy the Vulkan Frame Sync object and release sync objects.
     */
    ~VulkanFrameSync();

    VulkanFrameSync(const VulkanFrameSync&) = delete;
    VulkanFrameSync& operator=(const VulkanFrameSync&) = delete;

    /**
     * @brief Allocates synchronization semaphores and fences.
     * @param device Vulkan logical device context.
     * @param maxFramesInFlight Maximum frame buffers in flight concurrently.
     */
    void create(VkDevice device, uint32_t maxFramesInFlight = 2);
    /**
     * @brief Safely destroys synchronization primitives.
     */
    void destroy();

    /**
     * @brief Gets image acquisition semaphore.
     * @return VkSemaphore handle.
     */
    VkSemaphore getImageAvailableSemaphore() const { return imageAvailableSemaphores[currentFrame]; }
    /**
     * @brief Gets render completion semaphore.
     * @return VkSemaphore handle.
     */
    VkSemaphore getRenderFinishedSemaphore() const { return renderFinishedSemaphores[currentFrame]; }
    /**
     * @brief Gets current in-flight queue submission fence.
     * @return VkFence handle.
     */
    VkFence getInFlightFence() const { return inFlightFences[currentFrame]; }

    /**
     * @brief Blocks host execution until current frame fence is signaled, then resets it.
     */
    void waitForCurrentFrame();
    /**
     * @brief Progresses the frame index counter.
     */
    void nextFrame();

    /**
     * @brief Gets current active double-buffered index.
     * @return Index value.
     */
    uint32_t getCurrentFrameIndex() const { return currentFrame; }

private:
    /** @brief Reference to logical device. */
    VkDevice device = VK_NULL_HANDLE;
    /** @brief Max frames allowed in flight. */
    uint32_t maxFrames = 0;
    /** @brief Active frame index. */
    uint32_t currentFrame = 0;

    /** @brief Semaphores waiting for image availability. */
    std::vector<VkSemaphore> imageAvailableSemaphores;
    /** @brief Semaphores waiting for render passes to complete. */
    std::vector<VkSemaphore> renderFinishedSemaphores;
    /** @brief Fences waiting for queue work execution completion. */
    std::vector<VkFence> inFlightFences;
};
