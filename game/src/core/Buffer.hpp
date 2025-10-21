#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include "../include/ecs/components/Mesh.hpp"

class VulkanBuffer {
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceMemory memory = VK_NULL_HANDLE;

public:
    VulkanBuffer() = default;

    VulkanBuffer(VkDevice dev, VkBuffer buf, VkDeviceMemory mem)
        : device(dev), buffer(buf), memory(mem) {}

    ~VulkanBuffer() {
        if (buffer) vkDestroyBuffer(device, buffer, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
    }

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // ✅ Proper move constructor
    VulkanBuffer(VulkanBuffer&& other) noexcept
        : device(other.device), buffer(other.buffer), memory(other.memory)
    {
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
    }

    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept {
        if (this != &other) {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);

            device = other.device;
            buffer = other.buffer;
            memory = other.memory;

            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.device = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkBuffer get() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }

    // --- Static helpers ---

    static VulkanBuffer createBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    ) {
        VulkanBuffer buf;
        buf.device = device;
        buf.size = size;

        // --- Create buffer ---
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buf.buffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create buffer!");

        // --- Get memory requirements ---
        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device, buf.buffer, &memReq);

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

        uint32_t memoryTypeIndex = UINT32_MAX;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((memReq.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
                memoryTypeIndex = i;
                break;
            }
        }

        if (memoryTypeIndex == UINT32_MAX)
            throw std::runtime_error("Failed to find suitable memory type!");

        // --- Allocate memory ---
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(device, &allocInfo, nullptr, &buf.memory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate buffer memory!");

        if (vkBindBufferMemory(device, buf.buffer, buf.memory, 0) != VK_SUCCESS)
            throw std::runtime_error("Failed to bind buffer memory!");

        std::cout << "[createBuffer] Size=" << size
            << " Usage=" << usage
            << " MemoryType=" << memoryTypeIndex
            << " Flags=" << properties
            << " -> Buffer=" << buf.buffer
            << std::endl;

        return buf;
    }


    static VulkanBuffer createUniformBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size
    ) {
        return createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    static VulkanBuffer createStorageBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size
    ) {
        return createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }

    static VulkanBuffer createVertexBuffer(VkDevice device, VkPhysicalDevice phys, const std::vector<Vertex>& vertices) {
        VkDeviceSize size = sizeof(Vertex) * vertices.size();
        VulkanBuffer buf = createBuffer(
            device, phys, size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        void* data;
        vkMapMemory(device, buf.getMemory(), 0, size, 0, &data);
        memcpy(data, vertices.data(), (size_t)size);
        vkUnmapMemory(device, buf.getMemory());
        return buf;
    }

    static VulkanBuffer createIndexBuffer(VkDevice device, VkPhysicalDevice phys, const std::vector<uint32_t>& indices) {
        VkDeviceSize size = sizeof(uint32_t) * indices.size();
        VulkanBuffer buf = createBuffer(
            device, phys, size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        void* data;
        vkMapMemory(device, buf.getMemory(), 0, size, 0, &data);
        memcpy(data, indices.data(), (size_t)size);
        vkUnmapMemory(device, buf.getMemory());
        return buf;
    }
    // --- Copy data from one buffer to another (GPU-side, via command buffer) ---
    static void copyBuffer(
        VkDevice device,
        VkQueue transferQueue,
        VkCommandPool commandPool,
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size
    ) {
        // Allocate a temporary command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate copy command buffer!");

        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Define copy region
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        // Submit to transfer queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(transferQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    void destroy() {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
        device = VK_NULL_HANDLE;
    }
};