#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring>

/**
 * @class VulkanBuffer
 * @brief Inline wrapper class for managing a Vulkan buffer and its associated device memory.
 */
class VulkanBuffer {
public:
    /** @brief Vulkan buffer handle. */
    VkBuffer buffer = VK_NULL_HANDLE;
    /** @brief Vulkan device memory handle. */
    VkDeviceMemory memory = VK_NULL_HANDLE;
    /** @brief Reference to logical device. */
    VkDevice device = VK_NULL_HANDLE;
    /** @brief Size of the buffer in bytes. */
    VkDeviceSize size = 0;

    /**
     * @brief Default constructor.
     */
    VulkanBuffer() = default;

    /**
     * @brief Construct and initialize a new Vulkan Buffer object.
     * @param dev Logical device context.
     * @param phys Physical device.
     * @param bufferSize Size of the buffer.
     * @param usage Usage flags.
     * @param properties Memory properties.
     */
    VulkanBuffer(VkDevice dev, VkPhysicalDevice phys,
        VkDeviceSize bufferSize,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties)
        : device(dev), size(bufferSize)
    {
        create(dev, phys, bufferSize, usage, properties);
    }

    /**
     * @brief Construct a Vulkan Buffer object wrapping existing Vulkan resources.
     * @param dev Logical device.
     * @param buf Existing Vulkan buffer.
     * @param mem Existing Vulkan device memory.
     */
    VulkanBuffer(VkDevice dev, VkBuffer buf, VkDeviceMemory mem)
        : device(dev), buffer(buf), memory(mem) {
    }

    /**
     * @brief Destroy the Vulkan Buffer object and releases Vulkan handles.
     */
    ~VulkanBuffer() {
        destroy();
    }

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    /**
     * @brief Move constructor.
     * @param other Other buffer.
     */
    VulkanBuffer(VulkanBuffer&& other) noexcept {
        *this = std::move(other);
    }

    /**
     * @brief Move assignment operator.
     * @param other Other buffer.
     * @return Reference to this.
     */
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
    /**
     * @brief Creates and allocates the Vulkan buffer.
     * @param dev Logical device.
     * @param phys Physical device.
     * @param bufferSize Size of buffer.
     * @param usage Usage flags.
     * @param properties Memory properties.
     */
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
    /**
     * @brief Uploads data to host-visible memory in the buffer.
     * @param srcData Source pointer.
     * @param dataSize Size of data to transfer.
     * @param offset Transfer offset.
     */
    void upload(const void* srcData, VkDeviceSize dataSize, VkDeviceSize offset = 0) {
        if (dataSize == 0) return;
        if (!memory) throw std::runtime_error("Buffer memory not allocated!");
        void* dst;
        vkMapMemory(device, memory, offset, dataSize, 0, &dst);
        std::memcpy(dst, srcData, static_cast<size_t>(dataSize));
        vkUnmapMemory(device, memory);
    }

    // --- Destroy safely ---
    /**
     * @brief Frees the buffer and associated memory.
     */
    void destroy() {
        if (buffer) {
            if (device) {
                vkDeviceWaitIdle(device);
            }
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    /**
     * @brief Gets raw Vulkan buffer.
     * @return Vulkan buffer.
     */
    VkBuffer get() const { return buffer; }

private:
    /**
     * @brief Locates the correct memory type on physical device.
     * @param phys Physical device.
     * @param typeFilter Type filter mask.
     * @param properties Desired memory properties.
     * @return Memory type index.
     */
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
