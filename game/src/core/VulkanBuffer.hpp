#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring>

class VulkanBuffer {
public:
    VulkanBuffer() = default;
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    void create(VkDevice device, VkPhysicalDevice physicalDevice,
        VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties);

    void destroy();

    void uploadData(const void* srcData, VkDeviceSize dataSize);

    VkBuffer get() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }
    VkDeviceSize getSize() const { return bufferSize; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = 0;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
