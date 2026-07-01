#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <iostream>

/**
 * @class VulkanDevice
 * @brief Encapsulates Vulkan Instance creation, physical device selection, queue family discovery, and logical device setup.
 */
class VulkanDevice {
public:
    /**
     * @brief Construct a new Vulkan Device object.
     */
    VulkanDevice();
    /**
     * @brief Construct a new Vulkan Device object specifying validation layers settings.
     * @param enableValidation Whether validation checks are active.
     */
    VulkanDevice(bool enableValidation);
    /**
     * @brief Destroy the Vulkan Device object.
     */
    ~VulkanDevice();

    /**
     * @brief Initializes Vulkan instance context.
     */
    void initialize();
    /**
     * @brief Releases Vulkan logical device and instance handles.
     */
    void cleanup();

    // Getters
    /**
     * @brief Gets raw Vulkan Instance.
     * @return VkInstance handle.
     */
    VkInstance getInstance() const { return instance; }
    /**
     * @brief Gets picked physical device.
     * @return VkPhysicalDevice handle.
     */
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    /**
     * @brief Gets raw Vulkan logical device.
     * @return VkDevice handle.
     */
    VkDevice getDevice() const { return device; }
    /**
     * @brief Gets logical graphics queue.
     * @return VkQueue handle.
     */
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    /**
     * @brief Gets selected graphics queue family index.
     * @return Index value.
     */
    uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    /**
     * @brief Picks a physical GPU supporting graphics and surface presentation.
     * @param surface Window presentation surface.
     */
    void pickPhysicalDevice(VkSurfaceKHR surface);
    /**
     * @brief Instantiates logical device context and loads queue interfaces.
     */
    void createLogicalDevice();

    /**
     * @brief Searches physical memory capabilities for a matching property filter.
     * @param typeFilter Bitmask filtering memory types.
     * @param properties Desired memory properties.
     * @return Memory type index.
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

private:
    /** @brief Validation layers activity status. */
    bool enableValidationLayers = true;
    /** @brief Vulkan root instance. */
    VkInstance instance = VK_NULL_HANDLE;
    /** @brief Selected GPU physical device. */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    /** @brief Vulkan logical device interface. */
    VkDevice device = VK_NULL_HANDLE;
    /** @brief logical graphics queue handle. */
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    /** @brief graphics queue family index. */
    uint32_t graphicsQueueFamily = 0;

private:
    /**
     * @brief Creates the main Vulkan instance.
     */
    void createInstance();
    /**
     * @brief Verifies if physical device supports both graphics and surface presentation.
     * @param dev Physical device.
     * @param surface Presentation surface.
     * @return True if suitable, false otherwise.
     */
    bool isDeviceSuitableForSurface(VkPhysicalDevice dev, VkSurfaceKHR surface);
};
