#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

/**
 * @class VulkanDescriptors
 * @brief Manages the descriptor pool, descriptor layouts, and descriptor set allocations for UBO resources.
 */
class VulkanDescriptors {
public:
    /**
     * @brief Construct a new Vulkan Descriptors object.
     */
    VulkanDescriptors() = default;
    /**
     * @brief Destroy the Vulkan Descriptors object and release descriptor resources.
     */
    ~VulkanDescriptors();

    VulkanDescriptors(const VulkanDescriptors&) = delete;
    VulkanDescriptors& operator=(const VulkanDescriptors&) = delete;

    // Initialize descriptor system
    /**
     * @brief Creates the descriptor pool.
     * @param device Vulkan logical device context.
     * @param maxFramesInFlight Allocated size constraints for double-buffering.
     */
    void create(VkDevice device, uint32_t maxFramesInFlight = 2);

    // Create layout for a uniform buffer (e.g., camera UBO)
    /**
     * @brief Creates descriptor set layouts defining resource bindings for camera.
     */
    void createCameraDescriptorSetLayout();

    // Allocate and update descriptor sets for a uniform buffer
    /**
     * @brief Allocates and binds descriptor sets for the camera UBO.
     * @param uniformBuffer The GPU buffer containing camera matrices.
     * @param bufferSize Size of UBO buffer.
     */
    void allocateCameraDescriptorSets(VkBuffer uniformBuffer, VkDeviceSize bufferSize);

    // Cleanup
    /**
     * @brief Safely destroys layout bindings and descriptor pools.
     */
    void destroy();

    // Getters
    /**
     * @brief Gets raw camera descriptor set layout.
     * @return VkDescriptorSetLayout handle.
     */
    VkDescriptorSetLayout getCameraDescriptorSetLayout() const { return cameraDescriptorSetLayout; }
    /**
     * @brief Gets raw camera descriptor set.
     * @return VkDescriptorSet handle.
     */
    VkDescriptorSet getCameraDescriptorSet() const { return cameraDescriptorSet; }
    /**
     * @brief Gets raw descriptor pool.
     * @return VkDescriptorPool handle.
     */
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }

private:
    /** @brief Reference to logical device. */
    VkDevice device = VK_NULL_HANDLE;

    /** @brief Descriptor pool allocator. */
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    /** @brief Descriptor set layout allocated for camera. */
    VkDescriptorSetLayout cameraDescriptorSetLayout = VK_NULL_HANDLE;
    /** @brief Descriptor set allocated for camera. */
    VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;
};
