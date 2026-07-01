#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

/**
 * @class VulkanContext
 * @brief Manages initialization and destruction of Vulkan Instance, physical/logical devices, and surface interfaces.
 */
class VulkanContext {
public:
    /** @brief Vulkan root instance handle. */
    VkInstance instance = VK_NULL_HANDLE;
    /** @brief Selected GPU physical device handle. */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    /** @brief Vulkan logical device context. */
    VkDevice device = VK_NULL_HANDLE;
    /** @brief Queue for command submissions to graphic pipeline. */
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    /** @brief Queue for command submissions to system present pipeline. */
    VkQueue presentQueue = VK_NULL_HANDLE;
    /** @brief Presentation surface coupling GLFW window and Vulkan. */
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    /**
     * @brief Sets up Vulkan instance, window surface, picks physical GPU and logical device context.
     * @param window Pointer to GLFW window.
     */
    void init(GLFWwindow* window);
    /**
     * @brief Releases Vulkan logical device, surface, and instance handles.
     */
    void cleanup();

private:
    /**
     * @brief Creates the main Vulkan instance.
     */
    void createInstance();
    /**
     * @brief Scans and picks the first suitable physical GPU.
     */
    void pickPhysicalDevice();
    /**
     * @brief Configures queue families and instantiates the logical device interface.
     */
    void createLogicalDevice();
};
