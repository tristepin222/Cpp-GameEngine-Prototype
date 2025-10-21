#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class FrameManager {
public:
    FrameManager(VkDevice dev, VkQueue graphicsQ, VkQueue presentQ, VkSwapchainKHR sc, uint32_t framesInFlight = 2);
    ~FrameManager();

    void init(VkCommandPool commandPool);
    VkResult beginFrame(uint32_t& imageIndex);
    void setSwapchain(VkSwapchainKHR sc) { swapchain = sc; } // add setter
    void endFrame(uint32_t imageIndex);
    VkSwapchainKHR getSwapchain() const { return swapchain; }

    VkCommandBuffer getCurrentCommandBuffer() const { return commandBuffers[currentFrame]; }

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkImage> swapchainImages;

    uint32_t currentFrame = 0;

private:
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t maxFramesInFlight;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFormat imageFormat;
    VkExtent2D extent;


};
