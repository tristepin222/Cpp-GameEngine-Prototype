#include "VulkanDevice.hpp"
#include <glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>

// Helper for validation (simple)
static std::vector<const char*> getRequiredExtensions(bool enableValidation) {
    uint32_t glfwExtCount = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> exts(glfwExt, glfwExt + glfwExtCount);

    if (enableValidation) {
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // "VK_EXT_debug_utils"
    }
    return exts;
}

// ------------------------
// Public Methods
// ------------------------

VulkanDevice::VulkanDevice(bool enableValidation) : enableValidationLayers(enableValidation) {}
VulkanDevice::VulkanDevice() : enableValidationLayers(true) {}

VulkanDevice::~VulkanDevice() {
    cleanup();
}

// initialize now only creates the VkInstance (not picking physical device / logical device)
void VulkanDevice::initialize() {
    createInstance();
    // NOTE: call pickPhysicalDevice(surface) and createLogicalDevice() later (after surface creation)
}

void VulkanDevice::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

// ------------------------
// Private / Public Helpers
// ------------------------

void VulkanDevice::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    auto extensions = getRequiredExtensions(enableValidationLayers);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validation layers
    std::vector<const char*> validationLayers;
    if (enableValidationLayers) {
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

// New: pick physical device using a VkSurfaceKHR, ensuring present support on same queue family as graphics
void VulkanDevice::pickPhysicalDevice(VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) throw std::runtime_error("No GPUs with Vulkan support found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        if (isDeviceSuitableForSurface(dev, surface)) {
            physicalDevice = dev;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU that supports presentation to the surface");
    }
}

// Helper that checks for a queue family that supports graphics and present to the provided surface
bool VulkanDevice::isDeviceSuitableForSurface(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) return false;

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, families.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        // require graphics bit
        if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;

        // check present support for the surface
        VkBool32 presentSupport = VK_FALSE;
        VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
        if (res != VK_SUCCESS) continue;
        if (presentSupport == VK_TRUE) {
            graphicsQueueFamily = i;
            return true;
        }
    }

    return false;
}

void VulkanDevice::createLogicalDevice() {
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Physical device not selected. Call pickPhysicalDevice(surface) first.");
    }

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
}

uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}
