#pragma once
#include <vulkan/vulkan.h>
#include <glfw3.h>
#include <vector>

class VulkanContext {
public:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0; // store for command pool creation

    void init(GLFWwindow* window);
    void cleanup();

private:
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();

private:
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    bool checkValidationLayerSupport();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger();
    void destroyDebugMessenger();

};
