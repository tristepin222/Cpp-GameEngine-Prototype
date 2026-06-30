#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring>

class VulkanBuffer {
public:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDeviceSize size = 0;

    VulkanBuffer() = default;

    VulkanBuffer(VkDevice dev, VkPhysicalDevice phys,
        VkDeviceSize bufferSize,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties)
        : device(dev), size(bufferSize)
    {
        create(dev, phys, bufferSize, usage, properties);
    }

    VulkanBuffer(VkDevice dev, VkBuffer buf, VkDeviceMemory mem)
        : device(dev), buffer(buf), memory(mem) {
    }

    ~VulkanBuffer() {
        destroy();
    }

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept {
        *this = std::move(other);
    }

    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            device = other.device;
            buffer = other.buffer;
            memory = other.memory;
            size = other.size;
            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
        }
        return *this;
    }

    // --- Create & allocate ---
    void create(VkDevice dev, VkPhysicalDevice phys, VkDeviceSize bufferSize,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    {
        device = dev;
        size = bufferSize;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(dev, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan buffer!");

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(dev, buffer, &memReq);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(phys, memReq.memoryTypeBits, properties);

        if (vkAllocateMemory(dev, &allocInfo, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate Vulkan buffer memory!");

        vkBindBufferMemory(dev, buffer, memory, 0);
    }

    // --- Write data to the buffer (host visible) ---
    void upload(const void* srcData, VkDeviceSize dataSize, VkDeviceSize offset = 0) {
        if (!memory) throw std::runtime_error("Buffer memory not allocated!");
        void* dst;
        vkMapMemory(device, memory, offset, dataSize, 0, &dst);
        std::memcpy(dst, srcData, static_cast<size_t>(dataSize));
        vkUnmapMemory(device, memory);
    }

    // --- Destroy safely ---
    void destroy() {
        if (buffer) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    VkBuffer get() const { return buffer; }

private:
    static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }
};
