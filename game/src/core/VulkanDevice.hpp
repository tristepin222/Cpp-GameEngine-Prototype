#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <iostream>

class VulkanDevice {
public:
    VulkanDevice();
    VulkanDevice(bool enableValidation);
    ~VulkanDevice();

    void initialize();
    void cleanup();

    // Getters
    VkInstance getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    void pickPhysicalDevice(VkSurfaceKHR surface);
    void createLogicalDevice();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

private:
    bool enableValidationLayers = true;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;

private:
    void createInstance();
    bool isDeviceSuitableForSurface(VkPhysicalDevice dev, VkSurfaceKHR surface);
};
