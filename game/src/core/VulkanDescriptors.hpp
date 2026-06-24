#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class VulkanDescriptors {
public:
    VulkanDescriptors() = default;
    ~VulkanDescriptors();

    VulkanDescriptors(const VulkanDescriptors&) = delete;
    VulkanDescriptors& operator=(const VulkanDescriptors&) = delete;

    // Initialize descriptor system
    void create(VkDevice device, uint32_t maxFramesInFlight = 2);

    // Create layout for a uniform buffer (e.g., camera UBO)
    void createCameraDescriptorSetLayout();

    // Allocate and update descriptor sets for a uniform buffer
    void allocateCameraDescriptorSets(VkBuffer uniformBuffer, VkDeviceSize bufferSize);

    // Cleanup
    void destroy();

    // Getters
    VkDescriptorSetLayout getCameraDescriptorSetLayout() const { return cameraDescriptorSetLayout; }
    VkDescriptorSet getCameraDescriptorSet() const { return cameraDescriptorSet; }
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }

private:
    VkDevice device = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout cameraDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;
};
