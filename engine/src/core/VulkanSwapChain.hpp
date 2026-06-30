#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <iostream>

class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    void initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t graphicsQueueFamily);
    void cleanup();

    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }
    VkRenderPass getRenderPass() const { return renderPass; }
    VkSwapchainKHR getSwapchain() const { return swapchain; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkFormat imageFormat{};
    VkExtent2D extent{};
    VkRenderPass renderPass = VK_NULL_HANDLE;

private:
    void createSwapchain(uint32_t graphicsQueueFamily);
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};
