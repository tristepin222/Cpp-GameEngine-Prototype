#include "core/VulkanContext.hpp"
#include "core/SwapchainManager.hpp"
#include "renderer/Renderer.hpp"
#include <glfw3.h>
#include <iostream>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "DOD Renderer", nullptr, nullptr);

    VulkanContext context;
    context.init(window);

    SwapchainManager swapchain;
    VkExtent2D extent{ 1280, 720 };
    swapchain.create(context.device, context.surface, extent);

    Renderer renderer(context.device, context.physicalDevice, extent);
    renderer.createCameraBuffer();
    renderer.createInstanceBuffer(1000); // Reserve for up to 1000 entities

    // Create a descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool);

    renderer.initDescriptorSets(descriptorPool);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // In real code: acquire swapchain image, record cmd buffer, submit + present
        std::cout << "Frame tick\n";
    }

    vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
    swapchain.cleanup(context.device);
    context.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
