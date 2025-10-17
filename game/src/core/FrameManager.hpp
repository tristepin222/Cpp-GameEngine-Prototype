#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class FrameManager {
public:
    FrameManager(VkDevice dev, VkQueue graphicsQ, VkQueue presentQ, VkSwapchainKHR sc, uint32_t framesInFlight = 2);
    ~FrameManager();

    void init(VkCommandPool commandPool);
    void beginFrame(uint32_t& imageIndex);
    void endFrame(uint32_t imageIndex);

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
