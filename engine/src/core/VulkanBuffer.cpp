#include "VulkanBuffer.hpp"

/**
 * @brief Destroy the Vulkan Buffer:: Vulkan Buffer object and release Vulkan handles.
 */
VulkanBuffer::~VulkanBuffer() {
    destroy();
}

/**
 * @brief Move constructor.
 * @param other Buffer to move from.
 */
VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept {
    *this = std::move(other);
}

/**
 * @brief Move assignment operator.
 * @param other Buffer to move.
 * @return Reference to self.
 */
VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        device = other.device;
        physicalDevice = other.physicalDevice;
        buffer = other.buffer;
        memory = other.memory;
        bufferSize = other.bufferSize;
        other.device = VK_NULL_HANDLE;
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
    }
    return *this;
}

/**
 * @brief Creates and allocates the buffer.
 * @param dev Vulkan logical device context.
 * @param phys Vulkan physical device.
 * @param size Buffer size.
 * @param usage Usage flags.
 * @param properties Memory properties.
 */
void VulkanBuffer::create(VkDevice dev, VkPhysicalDevice phys,
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties) {
    device = dev;
    physicalDevice = phys;
    bufferSize = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");

    vkBindBufferMemory(device, buffer, memory, 0);
}

/**
 * @brief Safely destroys the buffer and memory.
 */
void VulkanBuffer::destroy() {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

/**
 * @brief Uploads data to the buffer.
 * @param srcData Source pointer.
 * @param dataSize Transfer size.
 */
void VulkanBuffer::uploadData(const void* srcData, VkDeviceSize dataSize) {
    if (dataSize > bufferSize)
        throw std::runtime_error("uploadData() size exceeds buffer capacity");

    void* mapped;
    vkMapMemory(device, memory, 0, dataSize, 0, &mapped);
    std::memcpy(mapped, srcData, static_cast<size_t>(dataSize));
    vkUnmapMemory(device, memory);
}

/**
 * @brief Selects the correct memory type on physical device.
 * @param typeFilter Bitmask filtering memory types.
 * @param properties Desired memory properties.
 * @return Memory type index.
 */
uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}
