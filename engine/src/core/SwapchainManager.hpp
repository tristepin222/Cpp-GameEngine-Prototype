#pragma once
#include <vulkan/vulkan.h>
#include <vector>

/**
 * @class SwapchainManager
 * @brief Manages the lifecycle, image views, render pass, and framebuffers of a Vulkan Swapchain.
 */
class SwapchainManager {
public:
    /** @brief Vulkan swapchain handle. */
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    /** @brief Retrieved images from the swapchain. */
    std::vector<VkImage> images;
    /** @brief Formatted image views mapping to swapchain images. */
    std::vector<VkImageView> imageViews;
    /** @brief format type of swapchain images. */
    VkFormat imageFormat;
    /** @brief Size dimensions of swapchain images. */
    VkExtent2D extent;
    /** @brief Main render pass associated with swapchain frames. */
    VkRenderPass renderPass = VK_NULL_HANDLE;
    /** @brief Framebuffers wrapping swapchain image views. */
    std::vector<VkFramebuffer> framebuffers;

    /**
     * @brief Queries hardware capabilities, builds swapchain, views, render pass, and framebuffers.
     * @param device Vulkan logical device context.
     * @param physicalDevice Vulkan physical device.
     * @param surface Window rendering surface.
     * @param windowSize Extent target dimensions.
     */
    void create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkExtent2D windowSize);
    /**
     * @brief Frees all allocated swapchain resources.
     * @param device Logical device context.
     */
    void cleanup(VkDevice device);
};
