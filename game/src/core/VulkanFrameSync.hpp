#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class VulkanFrameSync {
public:
    VulkanFrameSync() = default;
    ~VulkanFrameSync();

    VulkanFrameSync(const VulkanFrameSync&) = delete;
    VulkanFrameSync& operator=(const VulkanFrameSync&) = delete;

    void create(VkDevice device, uint32_t maxFramesInFlight = 2);
    void destroy();

    VkSemaphore getImageAvailableSemaphore() const { return imageAvailableSemaphores[currentFrame]; }
    VkSemaphore getRenderFinishedSemaphore() const { return renderFinishedSemaphores[currentFrame]; }
    VkFence getInFlightFence() const { return inFlightFences[currentFrame]; }

    void waitForCurrentFrame();
    void nextFrame();

    uint32_t getCurrentFrameIndex() const { return currentFrame; }

private:
    VkDevice device = VK_NULL_HANDLE;
    uint32_t maxFrames = 0;
    uint32_t currentFrame = 0;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
};
