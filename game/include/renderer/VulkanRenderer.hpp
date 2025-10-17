#pragma once
#include <vulkan/vulkan.h>
#include <glfw3.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include "../ecs/components/Mesh.hpp"
#include "../ecs/components/Material.hpp"
#include "../ecs/components/Transform.hpp"
#include "../ecs/components/Camera.hpp"

struct CameraUBO {
    glm::mat4 viewProj;
};


class VulkanRenderer {
public:
    VulkanRenderer(GLFWwindow* window);
    ~VulkanRenderer();

    void beginFrame();
    void drawMesh(const Mesh& mesh, const Material& mat, const Transform& transform);
    void endFrame();
    void uploadMesh(Mesh& mesh);
    void cleanup();

    bool shouldClose() const;
    float getDeltaTime();



    // Getters for ECS RenderSystem
    VkRenderPass getRenderPass() const { return renderPass; }
    VkPipeline getPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    VkFramebuffer getCurrentFramebuffer() const;
    VkExtent2D getSwapchainExtent() const { return swapchainExtent; }
	GLFWwindow* getWindow() const { return window; }
    VkCommandBuffer getCurrentCommandBuffer() const;

    VkPipeline createPipelineForShaders(const std::string& vertPath, const std::string& fragPath);

    // Submit a single command buffer for immediate use and wait until done

	void updateCameraUBO(Camera& cam, CameraUBO& ubo);

	void createCameraUBO();
	void createCameraDescriptorPool();

    void submitOneTimeCommand(VkCommandBuffer cmd);

    VkDescriptorSet& getCameraDescriptorSet() { return cameraDescriptorSet; }
    const VkDescriptorSet& getCameraDescriptorSet() const { return cameraDescriptorSet; }


private:
    GLFWwindow* window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t currentFrame = 0;

    double lastTime = 0.0;

    // Renderer handles
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;  // one for now
    VkExtent2D swapchainExtent{}; // for viewport/scissor

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;

    // Camera UBO
    VkBuffer cameraBuffer = VK_NULL_HANDLE;
    VkDeviceMemory cameraBufferMemory = VK_NULL_HANDLE;
    VkDescriptorSetLayout cameraDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool;

private:
    void initVulkan();
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void allocateCommandBuffers();

    void createBuffer(VkDeviceSize, VkBufferUsageFlags, VkMemoryPropertyFlags,
        VkBuffer&, VkDeviceMemory&);
    uint32_t findMemoryType(uint32_t, VkMemoryPropertyFlags);

    void createRenderPass();
    void createPipeline();
    void createFramebuffer(); // basic single-buffer example

    void createSwapchain();   // NEW
    void createImageViews();
    void createCameraDescriptorSetLayout();
	void createCameraDescriptorSet();


    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> loadFile(const std::string& path);

};
