#pragma once
#include <vulkan/vulkan.h>
#include <vector>

/**
 * @class FrameManager
 * @brief Manages frame synchronization, swapchain image acquisition, queue submission, and command buffers.
 */
class FrameManager {
public:
    /**
     * @brief Construct a new Frame Manager object.
     * @param dev Logical Vulkan device.
     * @param graphicsQ Queue for rendering submissions.
     * @param presentQ Queue for presentation.
     * @param sc Target Vulkan swapchain.
     * @param framesInFlight Number of frames allowed in flight concurrently.
     */
    FrameManager(VkDevice dev, VkQueue graphicsQ, VkQueue presentQ, VkSwapchainKHR sc, uint32_t framesInFlight = 2);
    /**
     * @brief Destroy the Frame Manager object, releasing sync handles.
     */
    ~FrameManager();

    /**
     * @brief Allocates command buffers and creates semaphores and fences.
     * @param commandPool Parent Vulkan command pool.
     */
    void init(VkCommandPool commandPool);
    /**
     * @brief Prepares for frame drawing by waiting for the in-flight fence, acquiring the next swapchain image, and resetting command buffers.
     * @param imageIndex Out parameter for acquired image index.
     */
    void beginFrame(uint32_t& imageIndex);
    /**
     * @brief Submits command buffers to the graphics queue and presents the swapchain image.
     * @param imageIndex Acquired swapchain image index.
     */
    void endFrame(uint32_t imageIndex);

    /**
     * @brief Gets command buffer active for the current in-flight frame.
     * @return Current command buffer.
     */
    VkCommandBuffer getCurrentCommandBuffer() const { return commandBuffers[currentFrame]; }

private:
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapchain;
    uint32_t maxFramesInFlight;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkCommandBuffer> commandBuffers;

    uint32_t currentFrame = 0;
};
