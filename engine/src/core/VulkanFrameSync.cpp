#include "VulkanFrameSync.hpp"

/**
 * @brief Destroy the Vulkan Frame Sync:: Vulkan Frame Sync object.
 */
VulkanFrameSync::~VulkanFrameSync() {
    destroy();
}

/**
 * @brief Safely destroys synchronization semaphores and fences.
 */
void VulkanFrameSync::destroy() {
    for (size_t i = 0; i < maxFrames; i++) {
        if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        if (inFlightFences[i] != VK_NULL_HANDLE)
            vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
}

/**
 * @brief Creates synchronization objects (semaphores, fences) for in-flight frames.
 * @param dev Logical device context.
 * @param maxFramesInFlight Maximum frames allowed concurrently in flight.
 */
void VulkanFrameSync::create(VkDevice dev, uint32_t maxFramesInFlight) {
    device = dev;
    maxFrames = maxFramesInFlight;

    imageAvailableSemaphores.resize(maxFrames);
    renderFinishedSemaphores.resize(maxFrames);
    inFlightFences.resize(maxFrames);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start as signaled so first frame doesnt wait forever

    for (uint32_t i = 0; i < maxFrames; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

/**
 * @brief Blocks CPU thread until active frame fence is signaled, then resets it.
 */
void VulkanFrameSync::waitForCurrentFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);
}

/**
 * @brief Advances active frame buffer index.
 */
void VulkanFrameSync::nextFrame() {
    currentFrame = (currentFrame + 1) % maxFrames;
}

